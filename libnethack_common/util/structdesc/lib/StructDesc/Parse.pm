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
our $parsesafe = new Safe;
# Technically we don't need some of these, because .sd files have a
# smaller grammar than aimake.rules files do. But we may as well use the
# same list to ensure things work.
$parsesafe->permit_only(qw/null stub scalar pushmark const undef list
                           qr negate lineseq leaveeval anonlist anonhash
                           rv2sv sassign nextstate padany regcreset concat
                           stringify quotemeta rv2gv/);
sub _parse_perl_from_path {
    my $path = shift;
    my $rv;
    my $filecontents;


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
our $numsafe = new Safe;
$numsafe->permit_only(qw/
    null stub scalar pushmark const leaveeval padany lineseq
    multiply i_multiply divide i_divide add i_add subtract i_subtract
    left_shift right_shift bit_and bit_xor bit_or negate i_negate
    not complement lt i_lt gt i_gt ge i_ge eq i_eq ne i_ne ncmp i_ncmp/);

sub _eval_numerical_expression {
    my $expression = shift;
    my $errmsg = shift;
    my $rv;
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

# Given a hash generated from an .sd file, desugars enum/bitfield
# arrays into enum/bitfield dictionaries and desugars 'enum '
# tags (via mutation in place), and recursively generates
# references to:
# - All constant, enum constant, and bitfield constant definitions
# - All type refinements
# - All uses of type names
# - All structure definitions
# - All union definitions
# This uses return-by-reference in order to produce better error messages
# (because the error will be detected immediately rather than upon merge).
sub _find_grammar_components;
sub _find_grammar_components {
    my $dict = shift;
    my $unref_is_constant = shift;
    my $dict_is_refinement = shift;
    my $errmsg = shift;
    my $rv = shift;
    for my $key (keys %$dict) {

        # If this dictionary's a refinement, we look only at type,
        # element_type, key_type fields.

        $dict_is_refinement and $key !~ /^(?:key_|element_)?type$/
            and next;

        # Desugar enum tags.
        if ($key =~ s/^enum //) {
            $dict->{$key} = $dict->{"enum $key"};
            ref $dict->{$key} eq 'HASH' or croak
                "$errmsg: type 'enum $key' is tagged 'enum' but is ".
                "not a refinement";
            defined $dict->{$key}{type} and croak
                "$errmsg: type 'enum $key' is tagged 'enum' but has ".
                "a type given explicitly";
            $dict->{$key}{type} = 'enum';
            delete $dict->{"enum $key"};
        }

        if (ref $dict->{$key} eq 'HASH' && $dict->{$key}{type} &&
            $dict->{$key}{type} =~ m/^(?:enum|bitfield)$/) {

            # Desugar enum and bitfield values.
            if (ref $dict->{$key}{values} eq 'HASH') {
                # nothing to do yet
            } elsif (ref $dict->{$key}{values} eq 'ARRAY') {
                # desguar it
                my $sugaredvalues = $dict->{$key}{values};
                my %desugaredvalues = ();

                # "keys @$sugaredvalues" is clearer, but wasn't introduced until
                # 5.12, so we calculate the range manually for portability
                for my $nth (0 .. $#$sugaredvalues) {
                    # TODO: Check for float overflow. (You'd need a pretty
                    # large number of bitfield keys to overflow a float, but
                    # it is possible. Note that floats are 100% reliable both
                    # on small integers, and powers of 2, making Perl's
                    # default float-based arithmetic work just fine here.)
                    if ($dict->{$key}{type} eq 'bitfield') {
                        # 1 << $nth uses integer arithmetic, which can all
                        # too easily overflow. Do the equivalent with floats.
                        $desugaredvalues{$sugaredvalues->[$nth]} = 2 ** $nth;
                    } else {
                        $desugaredvalues{$sugaredvalues->[$nth]} = $nth;
                    }
                }
                $dict->{$key}{values} = \%desugaredvalues;
            } else {
                croak "$errmsg: type '$key' is an enum or bitfield, but ".
                    "has a missing or malformed 'values' refinement";
            }

            # Record the enum and bitfield values as constants.
            my $p = $dict->{$key}{enumprefix};
            defined $p or $p = '';
            my $s = $dict->{$key}{enumsuffix};
            defined $s or $s = '';

            for my $k (keys %{$dict->{$key}{values}}) {
                exists $rv->{constants}{"$p$k$s"} and
                    croak "$errmsg: constant '$p$k$s' is defined in " .
                    "multiple places (including enum/bitfield '$key')";
                $rv->{constants}{"$p$k$s"} = \ ($dict->{$key}{values}{$k});
            }
        }

        # Place the RHS of this definition into an appropriate category
        # (unless it's a [lower, upper] range, which doesn't get placed into
        # a category); and recurse into any types on the RHS.
        if ($key =~ s/^(struct|union) //) {

            my $type = $1;
            my $rvdict = ($1 eq 'struct' ? $rv->{structs} : $rv->{unions});

            ref $dict->{"$type $key"} eq 'HASH' or
                croak "$errmsg: $1 '$key' is not specified as a " .
                "dictionary of its members";

            exists $rvdict->{$key} and
                croak "$errmsg: $type '$key' is defined in multiple places";
            $rvdict->{$key} = $dict->{"$type $key"};

            # Recursively find grammar components for the struct or union.
            _find_grammar_components $dict->{"$type $key"}, 0, 0,
                "$errmsg: in $1 '$key'", $rv;

        } elsif (!ref $dict->{$key}) {

            if ($unref_is_constant) {
                # This error message is a little redundant, but it clarifies
                # that it's a constant definition where we found the duplicate,
                # not an enum or bitfield value.
                exists $rv->{constants}{$key} and
                    croak "$errmsg: constant '$key' is defined in " .
                    "multiple places (including constant definition '$key')";
                $rv->{constants}{$key} = \ ($dict->{$key});
            } else {
                # non-reference scalars are type names in this context,
                # rather than constant definitions
                push @{$rv->{typenameuses}}, \ ($dict->{$key});
            }

        } elsif (ref $dict->{$key} eq 'HASH') {

            push @{$rv->{refinements}}, $dict->{$key};

            # type, element_type, key_type are potentially type name uses, and
            # are potentially type refinements.  We process them via a recursive
            # call.
            defined $dict->{$key}{type} or croak
                "$errmsg: type refinement '$key' does not specify a ".
                "type to refine";

            _find_grammar_components $dict->{$key}, 0, 1,
                "$errmsg: in type refinement '$key'", $rv;
        }
    }
    
    return $rv;
}

# Given a path to a .sd file, outputs a processed version of it.
# The processing done is:
# - Incorporation of included files (_include)
# - Evaluation of constant expressions
# - Substitution of type names into types
# - Removing 'struct ', 'enum ', 'union ' prefixes
# - Marking each type with extra fields:
#   - _generate: whether to place this in an output file (false means that we're
#     going to reference a definition in some other file)
#   - _typename: a type to use by reference (optional, if present will be able
#     to represent all values of this type)
# - Splitting into "typedefs", "structs", "unions", "includes", and "constants"
#   (where "constants" includes enum/bitfield definitions)
# - Verifying that all refinements have legal values
# - Verifying that all required refinements exist
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
        "$path: error: malformed _include directive";

    # TODO: Includes

    my $components = {};
    _find_grammar_components $structdesc, 1, 0, "$path: error", $components;

    # We read the keys in an arbitrary order; sometimes they'll mention types we
    # don't know about. So we postpone a type in that case and scan them
    # repeatedly until all the types are found. If we can't handle a type at
    # all, we'll have an interation with postponed keys and no changes, that we
    # can error out on.
    #
    # This is a hash from values that haven't been processed, to reasons why
    # they weren't processed.
    my %unprocessed_keys;
    my $anychanges;

    # First, handle constants (and enum/bitfield values); we may need them when
    # parsing other things. _find_grammar_components checked for duplicates, so
    # all we have to do is evaluate them.
    $anychanges = 1;
    %unprocessed_keys = map {$_ => '?'} keys %{$components->{constants}};

    while ($anychanges && %unprocessed_keys) {
        $anychanges = 0;

        # We clear the list of unprocessed keys on every iteration and
        # rebuild it as we go.
        my @temp_keys = keys %unprocessed_keys;
        %unprocessed_keys = ();
        
        for my $key (@temp_keys) {
            my $value = ${$components->{constants}{$key}};
            
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
}

1;
