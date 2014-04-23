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
use File::Spec;
use Scalar::Util qw/looks_like_number/;
use Data::Dumper;

use constant structdesc_desc_file => 'structdesc.sd';

# The raw structdesc format of structdesc format.  This is needed early to avoid
# infinitely recursive definitions.
our $raw_structdesc_desc;
# And the builtin types extracted from it.
our %builtin_types;

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

sub _eval_constant_expression {
    my $constexpr = shift;
    my $errmsg = shift;
    my $constants = shift;

    defined $constexpr or croak "$errmsg: missing constant";

    # Special-case infinity.
    $constexpr eq '+inf' || $constexpr eq '-inf'
        and return $constexpr;

    $constexpr =~ s{([_a-zA-Z]+)}{
        defined($constants->{$1}) ? $constants->{$1} :
        croak "$errmsg: unknown constant '$1'"
    }eg;

    return _eval_numerical_expression $constexpr, $errmsg;
}

# Works out the correct order in which to order struct fields. The rules are:
# - First, all unpacked fields, largest to smallest, ties broken by the field
#   name;
# - Then, all packed fields. We sort them in order from large to small, ties
#   broken by field name. Then we repeatedly pick the first element on the list,
#   work out how much space is left to fill up an 8- or 16-bit space, then run
#   down the list until the space is full (or as full as it can get). Then we
#   order the resulting fields from largest to smallest, ties broken by earliest
#   found to latest found.
# These rules are not 100% reliable for packed fields, but then, it's an NP-
# complete problem, and they work pretty well in practice. The important thing
# is that they are 100% stable; the order will not change given the same
# input.
#
# We disallow packed fields wider than 16 bits because they might not fit in an
# "int", and we can't portably use types other than ints for bitfields.
#
# The return value is the total number of bits, followed by the field names in
# order.
sub _pack_struct {
    my $members = shift;
    my $errmsg = shift;
    my @fno = ();
    my $nbits = 0;
    
    my @ms = keys %$members;
    @ms = sort {$members->{$b}{_bits} <=> $members->{$a}{_bits} ||
                    $a cmp $b} @ms;

    my @packedms = ();
    for my $m (@ms) {
        $members->{$m}{packed} and push @packedms, $m and next;
        push @fno, $m;
        $nbits += $members->{$m}{_bits};
    }

    my @blocks16 = ();
    my @blocks8  = ();
    my $bitsrem = 0;
    my $curblock = undef;
    while (@packedms) {
        @ms = @packedms;
        @packedms = ();
        for my $m (@ms) {
            if ($bitsrem == 0) {
                # Start a new block.
                if ($members->{$m}{_bits} <= 8) {
                    $curblock = \@blocks8;
                    $bitsrem = 8;
                    $nbits += 8;
                } elsif ($members->{$m}{_bits} <= 16) {
                    $curblock = \@blocks16;
                    $bitsrem = 16;
                    $nbits += 16;
                } else {
                    croak "$errmsg: field '$m' is packed, but wider than ".
                        "16 bits (and thus might not fit in an 'int')";
                }
            }
            if ($members->{$m}{_bits} <= $bitsrem) {
                # Place onto the current block.
                $bitsrem -= $members->{$m}{_bits};
                push @$curblock, $m;
            } else {
                # Defer until the next cycle.
                push @packedms, $m;
            }
        }
    }

    return $nbits, @fno, @blocks16, @blocks8;
}

# Calculates the number of bits required to represent the given positive number.
# (Negative infinity is also a legal value; although negative infinity would
# technically be the correct return in such a case, we return 0.)
sub _bits_to_represent {
    my $num = shift;

    # Bignums aren't fully thought through yet. We should at least not crash on
    # them, and need a finite return value due to the way the bit-counting code
    # works. So we specify 63 value bits + sign bit (which isn't returned by
    # this function).
    $num >= '+inf' and return 63;

    my $bits = 0;
    my $lowest_unrepresentable = 1.0;
    while ($num >= $lowest_unrepresentable) {
        $bits++;
        $lowest_unrepresentable *= 2.0;
    }
    return $bits;
}

# Works out the number of bits required to express a type refinement.
# This function works on a structdesc type definition in unparsed form; i.e.
# enum constants must be expressed using their names.  However, constant
# expressions must have already been evaluated, and the ->{type} field
# desugared all the way down to a built-in.
#
# There are two return values:
# - The number of bits;
# - Whether the field is signed (i.e. an integer that's either unpacked or that
#   takes negative values.
sub _count_bits;
sub _count_bits {
    my $typedef = shift;
    my $errmsg = shift;
    my @sentinels = @_;

    if (!ref $typedef) {
        $typedef eq 'char' and return (8, undef);
        # We want a value for wchar_t between 16 (Windows) and 32 (Linux).
        # 21 is also the value it would use if packed, so we use that, plus
        # 1 due to the unknown signedness.
        $typedef eq 'wchar_t' and return (22, undef);

        # Enum and bitfield types are awkward, because depending on the
        # compiler, they could be as short as 8 bits wide (and signed or
        # unsigned).
        $typedef eq 'enum' and return (8, undef);
        $typedef eq 'bitfield' and return (8, undef);

        # croak for errors in the input; die for internal errors.
        die "Checking the size of non-integral builtin type or generic 'int'";
    }

    # TODO: handle list_name

    # References are a special case. A pointer could be 32 or 64 bits wide;
    # thus, sort between 32- and 64-bit integers by picking a number in between.
    defined $typedef->{reference} and return (48, 0, 0);

    # If we have enum or bitfield constants, calculate a min and max from those.
    # If we're an integer, we know our min and max.
    # For numeric dictionaries, we do the same calculations on the key type.
    my $intdef = $typedef;
    $typedef->{type} eq 'numeric_dictionary' and $intdef = $typedef->{key_type};

    my $min = defined $typedef->{min} ? $typedef->{min} : '+inf';
    my $max = defined $typedef->{max} ? $typedef->{max} : '-inf';

    if ($intdef->{values} and ref $intdef->{values} eq 'HASH') {
        for my $v (values %{$intdef->{values}}) {
            $v < $min and $min = $v;
            $v > $max and $max = $v;
        }
    }

    # We need to be able to represent the entire range, plus any sentinel values
    # ("nullable" and any "terminator" passed in as part of a recursive call).
    # While we're here, check that any "usually" is in range, and any sentinels
    # are out of range. We want to check a nullable before a terminator, because
    # the terminator has to be even further out of range than the nullable does
    # if we specify both.
    $typedef->{nullable} and unshift @sentinels, $typedef->{nullable};
    for my $sentinel (@sentinels) {
        if ($min > $sentinel) {
            $min = $sentinel;
        } elsif ($max < $sentinel) {
            $max = $sentinel;
        } else {
            croak "$errmsg: 'terminator' or 'usually' value is inside the ".
                "range of the type ($min <= $sentinel <= $max)";
        }
    }

    $typedef->{usually} and $typedef->{usually} < $min and
        croak "$errmsg: 'usually' value is outside range (" .
        $typedef->{usually} . " below minimum $min)";
    $typedef->{usually} and $typedef->{usually} > $max and
        croak "$errmsg: 'usually' value is outside range (" .
        $typedef->{usually} . " above maximum $min)";

    # If we're measuring relative to x, then a stored "0" actually means
    # "x". Thus, we want to add relative_to, if defined, to the min and
    # max, before working out the range.
    if ($typedef->{relative_to}) {
        looks_like_number $typedef->{relative_to} or
            croak "$errmsg: relative_to is not a number";
        $min += $typedef->{relative_to};
        $max += $typedef->{relative_to};
    }

    # Base case: enums, chars, wchar_ts
    #
    # If possible, we want to use the compiler's built-in values for these.
    # Often, it isn't possible. We can't rely on enums being larger than a
    # 'char'; we can't rely on a 'char' being wider than [1, 127] and capable
    # of representing 0; we can't rely on a wchar_t being wider than [1, 127]
    # and capable of representing 0, either, sadly.
    #
    # Compilers don't have built-in support for bitfields, so we treat those
    # the same way as integers.
    if (($typedef->{type} eq 'enum' || $typedef->{type} eq 'char' ||
         $typedef->{type} eq 'wchar_t') && $min >= 0 && $max <= 127) {
        # The built-in type works fine.
        return (8, undef);
    }

    # Base case: integers, bitfields, and enum/char/wchar_ts that were forced to
    # be integers due to sentinel issues or values outside the "safe range"
    # [1, 127].
    if ($typedef->{type} =~ m/^(?:enum|bitfield|int|char|wchar_t)$/) {

        if ($typedef->{packed} && $min >= 0) {
            my $bits = _bits_to_represent $max;
            return ($bits, undef);
        } else {
            # We care about the larger of $max and $min-1. (For instance,
            # a range of +127, -128 has both the max and min bounding us at
            # 8 bits; 7 bits for the negative numbers, and 7 bits for the
            # positive numbers, = 8 because we're on a log scale.)
            my $larger = $max;
            $larger < -$min+1 and $larger = -$min+1;
            my $bits = (_bits_to_represent $larger) + 1;

            if (!$typedef->{packed}) {
                # Pick an integer size that is very likely to actually exist,
                # because "packed" wasn't specified. In practice, powers of 2
                # from 8 to 64 almost inevitably exist on modern processors
                # (apart from the occasional DSP, but even that is worked around
                # using int_least*_t types).
                $bits <  8 and $bits = 8;
                $bits >  8 and $bits < 16 and $bits = 16;
                $bits > 16 and $bits < 32 and $bits = 32;
                $bits > 32 and $bits < 64 and $bits = 64;
                # If we have more than 64 bits, just return that literally.
            }

            return ($bits, 1);
        }
    }

    # The list cases. We have to re-count the element type, because of a
    # possible terminator.

    # Recursive case: association lists with a "maxlen"
    if ($typedef->{type} =~ /association_list$/) {
        $typedef->{maxlen} or croak
            "$errmsg: association type has neither 'reference' nor 'maxlen'";

        # Calculate the size of one struct element. 
        my ($bitsc, $signed, undef) =
            _count_bits {
                type => 'struct',
                members => { key_type => $typedef->{key_type},
                             element_type => $typedef->{element_type} } },
                "$errmsg: in association list element structure",
                ($typedef->{terminator} ? ($typedef->{terminator}) : ());

        # We give room for a terminator, but not for a count; that's handled
        # by the list_name handling.

        my $maxlen = $typedef->{maxlen};
        $typedef->{terminator} && $maxlen++; # give room for the terminator

        return ($bitsc * $maxlen, undef);
    }

    # Recursive case: lists with a "maxlen"
    # (This regex is safe because ->{type} must be a builtin.)
    if ($typedef->{type} =~ /list$/) {
        $typedef->{maxlen} or croak
            "$errmsg: list type has neither 'reference' nor 'maxlen'";

        # No arrays of bitfields allowed (without nesting them in structures)
        ref $typedef->{element_type} and
            $typedef->{element_type}{packed} and croak
            "$errmsg: cannot form a list of a packed type";

        my ($bitsc, $signed) =
            _count_bits $typedef->{element_type},
                "$errmsg: in field 'element_type'",
                ($typedef->{terminator} ? ($typedef->{terminator}) : ());

        my $maxlen = $typedef->{maxlen};
        $typedef->{terminator} && $maxlen++; # give room for the terminator

        return ($bitsc * $maxlen, undef);
    }

    # Base case: string, wstring_t with a maxlen
    # We calculate this like a terminated list of chars, or wchar_ts.
    # (The actual representation might be different, but this will at least
    # give a fixed approximation for the bit count.)
    if ($typedef->{type} eq 'string' || $typedef->{type} eq 'wstring_t') {
        return _count_bits {
            type => 'terminated_list',
            element_type => $typedef->{type} eq 'string' ? 'char' : 'wchar_t',
            maxlen => $typedef->{maxlen},
            terminator => 0,
        }, $errmsg;
    }

    # Recursive case: numeric dictionaries
    # We know the number of bits for one element, so just multiply by the size
    # of the key type.
    if ($typedef->{type} eq 'numeric_dictionary') {
        # If we have no legal values for the key, the dictionary is size 0,
        # rather than infinitely long.
        $max - $min eq '-inf' and ($max, $min) = (0, 1);
        return ($typedef->{element_type}{_bits} * ($max - $min + 1), undef);
    }

    # Recursive case: unions
    # We pick the largest type of any member, and mark unsigned (because this
    # isn't a number).
    if ($typedef->{type} eq 'union') {
        my $maxbits = 0;
        for my $member (values %{$typedef->{members}}) {
            $member->{_bits} > $maxbits and $maxbits = $member->{_bits};
        }
        return ($maxbits, undef);
    }

    # Recursive case: structs
    # We delegate to _pack_struct.
    if ($typedef->{type} eq 'struct') {
        my ($bits, undef) = _pack_struct $typedef->{members};
        return ($bits, undef);
    }

    die "_count_bits called with a non-fully-substituted type: " .
        Dumper($typedef);
}

# Given a hash generated from an .sd file, desugars enum/bitfield arrays into
# enum/bitfield dictionaries and desugars 'enum ' tags (via mutation in place),
# and recursively generates references to:
# - All constant, enum constant, and bitfield constant definitions
# - The enum or bitfield that defines all enum and bitfield constants
# - All type refinements
# - All non-opaque uses of type names
# - All opaque uses of type names
# - All structure definitions
# - All union definitions
# - All ranges
# This uses return-by-reference in order to produce better error messages
# (because the error will be detected immediately rather than upon merge).
sub _find_grammar_components;
sub _find_grammar_components {
    my $dict = shift;
    my $unref_is_constant = shift;
    my $dict_is_refinement = shift;
    my $errmsg = shift;
    my $rv = shift;
    my $enclosing_type = shift;
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
                $rv->{enumowner}{"$p$k$s"} = $dict_is_refinement ?
                    $enclosing_type : $key;
            }
        }

        # Place the RHS of this definition into an appropriate category; and
        # recurse into any types on the RHS.
        my $what = $unref_is_constant ? 'type definition' :
            $dict_is_refinement ? 'field' : 'member';
        my $em = "$errmsg: in $what '$key'";

        if ($key =~ s/^(struct|union) //) {

            my $type = $1;

            ref $dict->{"$type $key"} eq 'HASH' or
                croak "$errmsg: $type '$key' is not specified as a " .
                "dictionary of its members";

            exists $rv->{"${type}s"}{$key} and
                croak "$errmsg: $type '$key' is defined in multiple places";
            $dict->{$key} = $dict->{"$type $key"};
            delete $dict->{"$type $key"};
            push @{$rv->{"${type}s"}{$em}}, \ ($dict->{$key});

            # Recursively find grammar components for the struct or union.
            _find_grammar_components $dict->{$key}, 0, 0, $em, $rv;

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
                push @{$rv->{typenameuses}{$em}}, \ ($dict->{$key});
            }

        } elsif (ref $dict->{$key} eq 'HASH') {

            # "refinements" doesn't use an extra layer of indirection,
            # otherwise the refinements can get clobbered when we replace type
            # references with the types themselves
            push @{$rv->{refinements}{$em}}, $dict->{$key};

            # type, element_type, key_type are potentially type name uses, and
            # are potentially type refinements.  We process them via a recursive
            # call.
            defined $dict->{$key}{type} or croak
                "$errmsg: $what '$key' does not specify a type to refine";

            if ($dict->{$key}{opaque}) {
                ref $dict->{$key}{type} and croak
                    "$em: type is opaque, yet given explicitly";
                push @{$rv->{opaques}{$em}}, \ ($dict->{$key});
            } else {
                _find_grammar_components $dict->{$key}, 0, 1, $em, $rv, $key;
            }

        } elsif (ref $dict->{$key} eq 'ARRAY') {

            push @{$rv->{ranges}{$em}}, \ ($dict->{$key});

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
#   - _typenamehints: a list of type names we'd like to use for this type, if
#     possible
#   - _bits: the bitwidth needed to represent the type; structure fields have a
#     _bits that is the sum of the _bits of the individual fields; reference
#     types always have a _bits of 48 (so that they are sorted between 64-bit
#     and 32-bit integers); list types are calculated from their reference or
#     maxlen
#   - _signed: for integers; usually 1, except for packed types with nonnegative
#     minimum
# - Splitting into "typedefs", "structs", "unions", "includes", and "constants"
#   (where "constants" includes enum/bitfield definitions)
# - Converting into structdesc compiled format (e.g. association lists instead
#   of dictionaries)
# - Ordering structure members correctly
# - Verifying that all refinements have legal values
# - Verifying that all required refinements exist
#
# As an example, given the following .sd file:
# {
#     foo => [0, 10],
#     bar => { type => 'foo', usually => 4 },
#     baz => '1 + 1',
# }
# the output will be:
# {
#     includes => [],
#     constants => [ {name => 'baz', value => 2} ],
#     typedefs => [
#         { type => 'foo',
#           definition => { type => [0, 10], _generate => 1, _signed => 1,
#                           _bits => 8, _typenamehints => ['foo'] } },
#         { type => 'bar',
#           definition => { type => [0, 10], usually => 4, _signed => 1,
#                           _bits => 8, _generate => 1,
#                           _typenamehints => ['bar', 'foo'] } },
#     ],
# }
# and if converted to a C header file, it will look something like:
# typedef signed char foo;
# typedef foo bar;
#
# The format of the output is described in structdesc.sd. (The weird-looking
# format of 'constants' and 'typedefs' is because structdesc does not support
# hash tables.)

sub parse_structdesc_from_path {
    my $path = shift;         # path to parse
    my $includefiles = shift; # possibly empty include path/fileset
    my $structdesc = _parse_perl_from_path $path;
    ref $structdesc eq 'HASH' or croak
        "$path: error: badly formatted (outermost braces are missing)";

    my %rv = (typedefs => [], includes => [], constants => []);

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

    my %constantcache = ();
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
                defined($constantcache{$1}) ?
                    "(" . $constantcache{$1} . ")" :
                # Otherwise, postpone it.
                    do {$unprocessed_keys{$key} = $1; $1}
            }eg;
            
            $unprocessed_keys{$key} and next;

            $anychanges = 1;

            # Now evaluate the expression, and replace it in the original
            # file.
            $value = _eval_numerical_expression $value,
                "$path: error: could not evaluate constant '$key' ('$value')";
            ${$components->{constants}{$key}} = $value;
            $constantcache{$key} = $value;

            next;
        }
    }

    for my $k (sort {($components->{enumowner}{$a} || '') cmp
                     ($components->{enumowner}{$b} || '') ||
                     $a cmp $b} keys %constantcache) {
        if ($components->{enumowner}{$k}) {
            push @{$rv{constants}}, {name => $k,
                                     value => $constantcache{$k},
                                     owner => $components->{enumowner}{$k}};
        } else {
            push @{$rv{constants}}, {name => $k, value => $constantcache{$k}};
        }
        
    }

    if (%unprocessed_keys) {
        my ($key, $value, undef) = %unprocessed_keys;
        croak "$path: error: constant '$key' references " .
            "unknown constant '$value'";
    }

    # Parse simple numeric types.
    while (my ($em, $ranges) = each %{$components->{ranges}}) {
        for my $range (@$ranges) {

            scalar @$$range == 2 or croak "$em: malformed numeric range";

            $_ = _eval_constant_expression $_, $em, \%constantcache
                for @$$range;
            my ($min, $max) = @$$range;

            $min <= $max or croak
                "$em: minimum value $min is greater than maximum value $max";

            $$range = {min => $min, max => $max, type => 'int'};
            push @{$components->{refinements}{$em}}, $$range;
        }
    }

    # Desugar structures and unions.
    for my $type (qw/struct union/) {
        while (my ($em, $defs) = each %{$components->{"${type}s"}}) {
            for my $def (@$defs) {
                # If any builtins are used directly, replace them with a
                # refinement (so that all the struct member types eventually
                # become refinements).
                for my $k (keys %$$def) {
                    if (!ref $$def->{$k} && $builtin_types{$$def->{$k}}) {
                        $$def->{$k} = {type => $$def->{$k}};
                        push @{$components->{refinements}{$em}}, $$def->{$k};
                    }
                }

                $$def = {type => $type, members => $$def};
                push @{$components->{refinements}{$em}}, $$def;
            }
        }
    }

    # Move all the type definitions into one big hash.
    my %typedefs = ();
    for my $typename (keys %$structdesc) {

        # We've already processed constants.
        delete $structdesc->{$typename}, next
            if exists $constantcache{$typename};

        croak "$path: error: Attempt to redefine built-in type '$typename'" if
            $builtin_types{$typename};

        # Add the _generate and _typenamehints fields.
        $structdesc->{$typename}{_generate} =
            $structdesc->{$typename}{abstract} ? 0 : 1;
        $structdesc->{$typename}{_typenamehints} = 
            $structdesc->{$typename}{abstract} ? [] : [$typename];

        $typedefs{$typename} = \ ($structdesc->{$typename});
    }

    # Replace uses of type names with the actual types. These share with the
    # type definitions; this means that the updates don't have to be done in
    # any particular order.
    while (my ($em, $tnus) = each %{$components->{typenameuses}}) {
        for my $tnu (@$tnus) {
            # A built-in could have been desugared into a refinement
            # (ref $$nu), but won't have been if it was in a refinement
            # already, so we check both.
            next if ref $$tnu || $builtin_types{$$tnu};
            defined $typedefs{$$tnu} or croak "$em: unknown type '$$tnu'";

            # This is probably impossible, but bugs caused by this would be very
            # hard to track down, so paranoia to rule the case out...
            ref ${$typedefs{$$tnu}} eq 'HASH' or croak "$em: referenced type ".
                "'$$tnu' is not a builtin nor type refinement";

            $$tnu = ${$typedefs{$$tnu}};
        }
    }

    # Replace opaques with the built-in opaque type.
    while (my ($em, $opaques) = each %{$components->{opaques}}) {
        for my $opaque (@$opaques) {
            ${$opaque}->{opaque_reference_to} = ${$opaque}->{type};
            ${$opaque}->{type} = 'opaque';
            delete ${$opaque}->{opaque};
        }
    }    

    # We now loop over all the type refinements, trying to condense them down
    # into a single type declaration.  We use _bits as the field that records
    # whether a type has been processed.
    $anychanges = 1;
    my $anyproblems = undef;
    while ($anychanges) {
        $anychanges = 0;
        $anyproblems = undef;

        R: while (my ($em, $refinements) = each %{$components->{refinements}}) {
            for my $refinement (@$refinements) {

                # Sanity check for an error that keeps happening
                ref $refinement or
                    die "$em: internal error: refinement is not a hash";

                # Have we already processed this?
                exists $refinement->{_bits} and next;

                # We can't process this refinement if we haven't processed its
                # children types yet (and they aren't unrefined built-ins).
                ref $refinement->{$_} eq 'HASH' and
                    !exists $refinement->{$_}{_bits} and
                    ($anyproblems = "$em: in field '$_': circular definition"),
                    next R
                    for qw/type key_type element_type/;

                if ($refinement->{members}) {
                    ref $refinement->{members}{$_} eq 'HASH' and
                        !exists $refinement->{members}{$_}{_bits} and
                        ($anyproblems = "$em: in member '$_': circular ".
                         "definition"), next R
                    for keys %{$refinement->{members}};
                }

                # Sanity check to make sure things have been properly processed
                # (to rule this out when debugging)
                ref $refinement->{type} && ref $refinement->{type}{type} and
                    die "_bits exists in non-fully-substituted type:" .
                    Dumper($refinement);

                $anychanges = 1;

                # Evaluate all constant expressions inside the type.
                # This is a bit of a hack, but we have to bootstrap somehow.
                while (my ($rkey, $rval) =
                       each %{$raw_structdesc_desc->{'struct refinement'}}) {
                    my $is_constexpr = undef;
                    if (ref $rval eq 'HASH') {
                        $is_constexpr =
                            ($rval->{type} =~ /constant_expression$/ ||
                             $rval->{type} eq 'nce');
                    } elsif (!ref $rval) {
                        $is_constexpr = ($rval =~ /constant_expression$/ ||
                                         $rval eq 'nce');
                    }
                    next unless $is_constexpr;
                    next unless $refinement->{$rkey};
                    $refinement->{$rkey} = _eval_constant_expression
                        $refinement->{$rkey}, "$em: in field '$rkey'",
                        \%constantcache;
                }
                
                my $rt = $refinement->{type};

                if (ref $rt) {
                    my @tnh = @{$refinement->{_typenamehints} || []};
                    push @tnh, @{$rt->{_typenamehints} || []};

                    # Copy the fields from $rt into this type, with the
                    # exception of _underscored fields and 'abstract'.
                    $_ !~ /^_/ and $_ ne 'abstract' and
                        $refinement->{$_} = $rt->{$_} for keys %$rt;

                    # Combine the type name hints.
                    $refinement->{_typenamehints} = \@tnh;
                }

                # Count the bits in the type, and whether we can reuse the
                # type name (if it was uncertain earlier).
                my ($bits, $signed, undef) =
                    _count_bits $refinement, $em;

                $refinement->{_bits} = $bits;
                $refinement->{_signed} = $signed ? 1 : 0;
            }
        }
    }

    defined $anyproblems and croak $anyproblems;

    # Convert types into structdesc format. The major change is replacement
    # of dictionaries with association lists: 'values' and 'members'.
    # Additionally, these have to be ordered; we order enum values in
    # numerical order, and struct members in the order suggested by
    # _pack_struct.
    while (my ($em, $refinements) = each %{$components->{refinements}}) {
        for my $refinement (@$refinements) {
            if ($refinement->{members}) {
                my (undef, @morder) = _pack_struct $refinement->{members};
                $refinement->{members} =
                    [map +{name => $_, type => $refinement->{members}{$_}},
                     @morder];
            }
            if ($refinement->{values}) {
                $refinement->{values} =
                    [map +{name => $_, value => $refinement->{values}{$_}},
                     sort {$refinement->{values}{$a} <=>
                           $refinement->{values}{$b}}
                     keys $refinement->{values}];
            }
        }
    }

    # Write the type definitions into our return value.
    for my $typename (sort keys %typedefs) {
        push @{$rv{typedefs}}, {type => $typename,
                                definition => ${$typedefs{$typename}}};
    }

    return \%rv;
}

# At 'require' time, load the structdesc for structdesc itself.  We look for it
# in @INC.

for my $incprefix (@INC) {
    ref $incprefix and next; # ignore import hooks, use scalars only
    my $fn = File::Spec->catfile($incprefix, structdesc_desc_file);
    -f $fn or next;
    $raw_structdesc_desc = _parse_perl_from_path $fn;
    ref $raw_structdesc_desc eq 'HASH' or croak
        "$fn: error: badly formatted (missing outermost braces)";
}

$raw_structdesc_desc or croak "Couldn't find " . structdesc_desc_file . ' in @INC';

# Load the names of built-in types.
my $typevalues = $raw_structdesc_desc->{'enum builtin_type'}->{values};
$typevalues or croak structdesc_desc_file .
    ": error: missing 'enum builtin_type' or its 'values'";
$builtin_types{$_} = 1 for @$typevalues;

1;
