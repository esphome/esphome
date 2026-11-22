#!/bin/bash
# count.sh CAPTURE: jsn_sr04t counters from the first request to just before the 61st, plus the last tally lines.
awk '/Request read out/{r++; if(r>60) exit} r>0 && /Read byte from sensor/{b++} r>0 && /Distance from sensor/{d++} r>0 && /checksum failed/{c++} r>0 && /Invalid data/{i++} END{printf "requests=%d bytes=%d readings=%d checksum_failed=%d out_of_range=%d\n", (r>60?60:r), b, d, c, i}' "$1"
grep -a -E "RX decoder|ESPHome version|TALLY" "$1" | tail -4
