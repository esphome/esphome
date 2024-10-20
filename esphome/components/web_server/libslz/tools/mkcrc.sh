#!/bin/bash

# This script emits a CRC32 table suitable for use with rfc1952-based encoders.
# It's a very simple reimplementation of the C version in shell. Comments about
# the style will go to /dev/null.

declare -a crc0 crc1 crc2 crc3
for n in {0..255}; do
	c=$((n^255))
	for k in {0..7}; do
		# c=((c&1)?0xedb88320:0) ^ (c >> 1)
		c=$(((-(c & 1) & 0xedb88320) ^ (c >> 1)))
	done
	crc0[$n]=$((c ^ 0xff000000))
done

for n in {0..255}; do
	crc1[$n]=$((0xff000000 ^ crc0[(0xff000000 ^ crc0[$n] ^ 0xff) & 0xff] ^ (crc0[$n] >> 8)));
        crc2[$n]=$((0xff000000 ^ crc0[(0x00ff0000 ^ crc1[$n] ^ 0xff) & 0xff] ^ (crc1[$n] >> 8)));
        crc3[$n]=$((0xff000000 ^ crc0[(0x0000ff00 ^ crc2[$n] ^ 0xff) & 0xff] ^ (crc2[$n] >> 8)));
done

printf "static const uint32_t crc32_fast[4][256] = {\n"
printf "\t{ // [0]\n"
for n in {0..255}; do
	((n & 3)) || printf "\t\t"
	printf "0x%08x," ${crc0[$n]}
	if (((n & 3) == 3)); then
		printf " // %d-%d\n" $((n-3)) $n
	else
		printf " "
	fi
done
printf "\t},\n"

printf "\t{ // [1]\n"
for n in {0..255}; do
	((n & 3)) || printf "\t\t"
	printf "0x%08x," ${crc1[$n]}
	if (((n & 3) == 3)); then
		printf " // %d-%d\n" $((n-3)) $n
	else
		printf " "
	fi
done
printf "\t},\n"

printf "\t{ // [2]\n"
for n in {0..255}; do
	((n & 3)) || printf "\t\t"
	printf "0x%08x," ${crc2[$n]}
	if (((n & 3) == 3)); then
		printf " // %d-%d\n" $((n-3)) $n
	else
		printf " "
	fi
done
printf "\t},\n"

printf "\t{ // [3]\n"
for n in {0..255}; do
	((n & 3)) || printf "\t\t"
	printf "0x%08x," ${crc3[$n]}
	if (((n & 3) == 3)); then
		printf " // %d-%d\n" $((n-3)) $n
	else
		printf " "
	fi
done
printf "\t}\n"
printf "};\n"
