#!/bin/bash

# This script emits a fixed Huffman distance table of all 32768 possible
# match distances. It's a port of the initial C version.

# Distance codes are stored on 5 bits reversed. The RFC doesn't state that
# they are reversed, but it's the only way it works.
dist_codes=( 0 16 8 24 4 20 12 28 2 18 10 26 6 22 14 30 1 17 9 25 5 21 13 29 3 19 11 27 7 23 15 31 )

# Returns in $REPLY code for length $1 from 1 to 32768. The bit size for the
# next value can be found this way :
#      bits = code >> 1;
#      if (bits)
#              bits--;
dist_to_code() {
	if [ $1 -gt 24576 ]; then
		REPLY=29
	elif [ $1 -gt 16384 ]; then
		REPLY=28
	elif [ $1 -gt 12288 ]; then
		REPLY=27
	elif [ $1 -gt 8192 ]; then
		REPLY=26
	elif [ $1 -gt 6144 ]; then
		REPLY=25
	elif [ $1 -gt 4096 ]; then
		REPLY=24
	elif [ $1 -gt 3072 ]; then
		REPLY=23
	elif [ $1 -gt 2048 ]; then
		REPLY=22
	elif [ $1 -gt 1536 ]; then
		REPLY=21
	elif [ $1 -gt 1024 ]; then
		REPLY=20
	elif [ $1 -gt 768 ]; then
		REPLY=19
	elif [ $1 -gt 512 ]; then
		REPLY=18
	elif [ $1 -gt 384 ]; then
		REPLY=17
	elif [ $1 -gt 256 ]; then
		REPLY=16
	elif [ $1 -gt 192 ]; then
		REPLY=15
	elif [ $1 -gt 128 ]; then
		REPLY=14
	elif [ $1 -gt 96 ]; then
		REPLY=13
	elif [ $1 -gt 64 ]; then
		REPLY=12
	elif [ $1 -gt 48 ]; then
		REPLY=11
	elif [ $1 -gt 32 ]; then
		REPLY=10
	elif [ $1 -gt 24 ]; then
		REPLY=9
	elif [ $1 -gt 16 ]; then
		REPLY=8
	elif [ $1 -gt 12 ]; then
		REPLY=7
	elif [ $1 -gt 8 ]; then
		REPLY=6
	elif [ $1 -gt 6 ]; then
		REPLY=5
	elif [ $1 -gt 4 ]; then
		REPLY=4
	elif [ $1 -gt 3 ]; then
		REPLY=3
	elif [ $1 -gt 2 ]; then
		REPLY=2
	elif [ $1 -gt 1 ]; then
		REPLY=1
	else
		REPLY=0
	fi
}

printf "static const uint32_t fh_dist_table[32768] = {\n"
for dist in {0..32767}; do
	dist_to_code $((dist + 1))
	code=$REPLY
	bits=$(((code >> 1) ? (code >> 1) - 1 : 0))
	code=${dist_codes[$code]}
	code=$((code + ((dist & ((1 << (bits)) - 1)) << (5))))
	code=$(((code << (5)) + bits + 5))

	((dist & 7)) || printf "\t"
	printf "0x%08x," $code
	if (((dist & 7) == 7)); then
		printf " // %d-%d\n" $((dist-7)) $dist
	else
		printf " "
	fi
done
printf "};\n"
