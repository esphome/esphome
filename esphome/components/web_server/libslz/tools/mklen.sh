#!/bin/bash
# builds the length code table for any length

len=3;
off=0;
code=257;
while [[ $len -le 258 ]]; do
	if [[ $len -le 10 ]]; then
		bits=0;
	elif [[ $len -le 18 ]]; then
		bits=1;
	elif [[ $len -le 34 ]]; then
		bits=2;
	elif [[ $len -le 66 ]]; then
		bits=3;
	elif [[ $len -le 130 ]]; then
		bits=4;
	elif [[ $len -le 257 ]]; then
		bits=5;
	else
		# 258 skips (284,31)
		code=$((code+1))
		off=0;
		bits=0;
	fi
	##printf "code=%d bits=%d val=%d len=%d\n" $code $bits $off $len
	##printf "{ .code=%d, .bits=%d, .val=%d }, // len=%d\n" $code $bits $off $len

	printf "0x%04x, " $(( (code-257) + (bits*32) + (off * 256) ))

	off=$(((off + 1) & ((1 << bits) - 1))); #
	if [[ $off -eq 0 ]]; then
		code=$((code + 1))
	fi
	len=$((len + 1))
done
