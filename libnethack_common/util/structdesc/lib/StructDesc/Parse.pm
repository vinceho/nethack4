#!/usr/bin/perl
use utf8;     # this source file is UTF-8
use warnings;
use strict;
use 5.8.3;

# StructDesc::Parse: structure description language parser
# Copyright © 2014 Alex Smith.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# A copy of version 3 of the GNU General Public License can be viewed
# online at <http://www.gnu.org/licenses/>. If you obtained
# structdesc.pl as part of NetHack 4, you can also view this license
# as the file libnethack/dat/gpl3.
#
# Alternatively, at your option, you can redistribute and/or modify this
# program under the terms of the NetHack General Public License; see
# libnethack/dat/license for the details. Regardless of which license you
# choose, this program is distributed WITHOUT ANY WARRANTY.

package StructDesc::Parse;
use Exporter 'import';

our @EXPORT_OK = qw/parse_structdesc parse_structdesc_from_path
                    locate_structdesc_file list_structdesc_dependencies/;

use Safe;
use Carp;

# We use the same algorithm as aimake does for parsing a .sd file: we load
# it in a Safe and eval it. This reduces the number of Perl dependencies
# needed.
sub _parse_perl_from_path {
    my $path = shift;
    my $parsesafe = new Safe;
    my $rv;
    my $filecontents;

    # Technically we don't need some of these, because .sd files have a
    # smaller grammar than aimake.rules files do. But we may as well use the
    # same list to ensure things work.
    $parsesafe->permit_only(qw/null stub scalar pushmark const undef list
                               qr negate lineseq leaveeval anonlist anonhash
                               rv2sv sassign nextstate padany regcreset concat
                               stringify quotemeta rv2gv/);

    {
        open my $fh, '<', $path
            or croak "$path: error: could not open: $!";
        local $/;
        $filecontents = <$fh>;
        close $fh;
    }

    undef $@;

    # Work around a bug in 5.8, as in aimake
    if ($] >= 5.009) {
        $rv = $parsesafe->reval($filecontents, 0);
    } else {
        $rv = eval $filecontents;
    }

    $@ or ($! and $@ = $!);
    $@ or $@ = "contains no definitions";
    $@ =~ s/\(eval \d+\) //g;
    defined $rv or croak "$path: error: $@";
    return $rv;
}

# Given a numerical expression, calculate its value. Croaks with $errmsg if
# something is wrong with the arguments.
sub _eval_numerical_expression {
    my $expression = shift;
    my $errmsg = shift;
    my $numsafe = new Safe;
    my $rv;
    $numsafe->permit_only(qw/
        null stub scalar pushmark const leaveeval padany lineseq
        multiply i_multiply divide i_divide add i_add subtract i_subtract
        left_shift right_shift bit_and bit_xor bit_or negate i_negate
        not complement lt i_lt gt i_gt ge i_ge eq i_eq ne i_ne ncmp i_ncmp/);
    undef $@;
    if ($] >= 5.009) {
        $rv = $numsafe->reval($expression, 0);
    } else {
        $rv = eval $expression;
    }
    $@ =~ s/\(eval \d+\) //g;
    defined $rv or croak "$errmsg: " . ($@ || 'unknown error');
    return $rv;
}

# Given a path to a .sd file, outputs a processed version of it.
# The processing done is:
# - Incorporation of included files (_include)
# - Evaluation of constants
# - Substitution of type names into types
# - Removing 'struct ', 'enum ', 'union ' prefixes
# - Marking each type with extra fields:
#   - _generate: whether to place this in an output file (false means that we're
#     going to reference a definition in some other file)
#   - _reference: a type to use by reference (optional, if present will be able
#     to represent all values of this type)
# - Splitting into "typedefs", "structs", "unions", "includes", and "constants"
#
# As an example, given the following .sd file:
# {
#     foo => [0, 10],
#     bar => { type => 'foo', usually => 4 },
# }
# the output will be:
# {
#     typedefs => {
#         foo => { type => [0, 10], _generate => 1 },
#         bar => { type => [0, 10], _generate => 1, _reference => 'foo' },
#     },
# }
# and if converted to a C header file, it will look something like:
# typedef signed char foo;
# typedef foo bar;

sub parse_structdesc_from_path {
    my $path = shift;         # path to parse
    my $includefiles = shift; # possibly empty include path/fileset
    my $structdesc = _parse_perl_from_path $path;
    ref $structdesc eq 'HASH' or croak
        "$path: error: badly formatted (outermost braces are missing)";

    my %rv = (typedefs => {}, structs => {}, unions => {},
              includes => {}, constants => {});

    my $includelist = $structdesc->{_include} || [];
    delete $structdesc->{_include};
    ref $includelist eq 'ARRAY' or croak
        ".sd file '$path' contains a malformed _include directive";

    # TODO: Includes

    # We read the keys in an arbitrary order; sometimes they'll
    # mention types we don't know about. So we postpone a type in that
    # case and scan them repeatedly until all the types are found. If
    # we can't handle a type at all, we'll have an interation with
    # postponed keys and no changes, that we can error out on.
    #
    # This is a hash from values that haven't been processed, to
    # reasons why they weren't processed.
    my %unprocessed_keys;
    my $anychanges;

    # First, handle constants; we may need them when parsing other
    # things.
    $anychanges = 1;
    %unprocessed_keys = map {$_ => '?'}
        grep {!ref $structdesc->{$_}} keys %$structdesc;
    while ($anychanges && %unprocessed_keys) {
        $anychanges = 0;

        # We clear the list of unprocessed keys on every iteration and
        # rebuild it as we go.
        my @temp_keys = keys %unprocessed_keys;
        %unprocessed_keys = ();
        
        for my $key (@temp_keys) {
            my $value = $structdesc->{$key};
            
            # It's a constant. Thus, any identifiers embedded inside it must
            # also be constants. We loop over the value, substituting
            # identifiers as we go.
            
            $value =~ s{([_a-zA-Z]+)}{
                # Infinity is special.
                $1 eq 'inf' ? 'inf' :
                # If we've seen this embedded identifier, substitute it.
                defined($rv{constants}{$1}) ?
                    "(" . $rv{constants}{$1} . ")" :
                # Otherwise, postpone it.
                    do {$unprocessed_keys{$key} = $1; $1}
            }eg;
            
            $unprocessed_keys{$key} and next;

            $anychanges = 1;

            # Now evaluate the expression. We leave the original value
            # untouched so as to be able to write "#define FOO (BAR+1)"
            # rather than "#define FOO 20"; the idea is to produce a
            # more informative output file.

            $value = _eval_numerical_expression $value,
                "$path: error: could not evaluate constant '$key' ('$value')";
            $rv{constants}{$key} = $value;

            next;
        }
    }

    if (%unprocessed_keys) {
        my ($key, $value, undef) = %unprocessed_keys;
        croak "$path: error: constant '$key' references " .
            "unknown constant '$value'";
    }

    return \%rv;

            # # If we have an 'enum ', 'struct ', or 'union ' tag, the
            # # value must be a hash.
            # $key =~ m/^(?:enum|struct|union )/ and ref $value ne 'HASH' and
            #     croak "$path: error: enum/struct/union key '$key' must " .
            #     "be specified in long form (with braces)";
            
            # # Desugar 'enum x' => {...} to 'x' => {type => 'enum', ...}
            # $key =~ s/^enum // and $value->{type} = 'enum';
            

}

1;
