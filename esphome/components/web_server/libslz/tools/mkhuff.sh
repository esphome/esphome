#!/bin/bash
# builds the fixed huffman code table
#         0 - 143     8          00110000 through  10111111
#       144 - 255     9         110010000 through 111111111
#       256 - 279     7           0000000 through   0010111
#       280 - 287     8          11000000 through  11000111
#
# Code is reversed
#  - bits 0..3  : bits
#  - bits 4..12 : code

code=0;
while [[ $code -lt 288 ]]; do
	if [[ $code -le 143 ]]; then
		bits=8;
		value=$((0x30+code-0))
	elif [[ $code -le 255 ]]; then
		bits=9;
		value=$((0x190+code-144))
	elif [[ $code -le 279 ]]; then
		bits=7;
		value=$((0x00+code-256))
	else
		bits=8;
		value=$((0xc0+code-280))
	fi

	bit=$bits
	out=0
	while [[ $bit -gt 0 ]]; do
		((bit--))
		out=$((out + (((value >> bit) & 1) << (bits-1-bit)))); #
	done

	# printf "code=%d bits=%d value=%03x out=%03x\n" $code $bits $value $out
	printf "0x%04x, " $((out * 16 + bits))

	((code++))
done
