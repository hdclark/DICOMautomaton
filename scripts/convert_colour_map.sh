#!/usr/bin/env bash

# Convert a colour map from discrete integer triples to floating-point DCMA format.
#
# Usage: cat input_rgb.txt | $0 > output.cc

declare -a rarr
declare -a garr
declare -a barr

max_r=255
max_g=255
max_b=255

while read r && read g && read b ; do
    #printf 'Read entry %s, %s, %s\n' "$r" "$g" "$b"
    rarr+=("$r")
    garr+=("$g")
    barr+=("$b")
done < <(  dos2unix `#   <----------- stdin is read in starting here. ` |
           tr -c '[[:alnum:]]' ' ' |
           sed -e 's/[\s]\{2,\}/ /g' |
           tr ' ' '\n' |
           grep -E -v '^\s*$' )

r_arr_length="${#rarr[@]}"
g_arr_length="${#garr[@]}"
b_arr_length="${#barr[@]}"
printf 'Read in %s, %s, %s entries.\n' "${r_arr_length}" "${g_arr_length}" "${b_arr_length}"

# red
printf '%s\n' "    const static samples_1D<double> r( {"
i=0
for c in "${rarr[@]}" ; do
    printf '        { %s, 0.0, %s, 0.0 },' \
      "$(awk "BEGIN {print ($i/($r_arr_length - 1))}")" \
      "$(awk "BEGIN {print ($c/$max_r)}")"
    [ $((i % 2)) -eq 1 ] && printf '\n'
    i=$((1 + i))
done
[ $((i % 2)) -eq 1 ] && printf '\n'
printf '%s\n' "    } );"

printf '\n'

# green
printf '%s\n' "    const static samples_1D<double> g( {"
i=0
for c in "${garr[@]}" ; do
    printf '        { %s, 0.0, %s, 0.0 },' \
      "$(awk "BEGIN {print ($i/($g_arr_length - 1))}")" \
      "$(awk "BEGIN {print ($c/$max_g)}")"
    [ $((i % 2)) -eq 1 ] && printf '\n'
    i=$((1 + i))
done
[ $((i % 2)) -eq 1 ] && printf '\n'
printf '%s\n' "    } );"

printf '\n'

# blue
printf '%s\n' "    const static samples_1D<double> b( {"
i=0
for c in "${barr[@]}" ; do
    printf '        { %s, 0.0, %s, 0.0 },' \
      "$(awk "BEGIN {print ($i/($b_arr_length - 1))}")" \
      "$(awk "BEGIN {print ($c/$max_b)}")"
    [ $((i % 2)) -eq 1 ] && printf '\n'
    i=$((1 + i))
done
[ $((i % 2)) -eq 1 ] && printf '\n'
printf '%s\n' "    } );"

