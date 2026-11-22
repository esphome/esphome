#!/bin/bash
# ana.sh CAPTURE: counts for an SCD30 run
export LC_ALL=C
f=$1
echo "== $f"
echo "boots: $(grep -a -c 'Running through setup' $f)  fw: $(grep -a -o 'Firmware v[0-9.]*' $f | sort | uniq -c | tr '\n' ' ')"
echo "readings: $(grep -a -c 'Got CO2' $f)"
echo "data-ready polls 02:02: $(grep -a -c '0x61 TX 02:02' $f)  read cmds 03:00: $(grep -a -c '0x61 TX 03:00' $f)"
# NACKs on data transfers to 0x61 from anywhere in the log; the bus scan's empty probes are skipped.
if grep -a -q ': 0x61 TX' $f; then
  nack=$(awk 'match($0, /]: 0x[0-9A-Fa-f][0-9A-Fa-f] (TX|RX)/) {
      a = substr($0, RSTART + 3, 4); rest = substr($0, RSTART + RLENGTH); gsub(/[ \r]/, "", rest)
      data = (a == "0x61" && rest != ""); next
    }
    /not acknowledged/ && data { n++ }
    END { print n + 0 }' $f)
else
  nack="no 0x61 transfers logged"
fi
echo "RX failed: $(grep -a -c 'RX .* failed' $f)  NACK on 0x61: $nack  TX timeout: $(grep -a -c 'TX failed: timeout' $f)  other TX fail: $(grep -a -c -E 'TX failed: (unknown|buffer)' $f)"
echo "CRC: $(grep -a -c 'CRC invalid' $f)  not ready: $(grep -a -c 'Data not ready' $f)  err reading: $(grep -a -c 'Error reading measurement' $f)  warn flag: $(grep -a -c -i 'warning flag' $f)  failed: $(grep -a -c -i 'marked FAILED\|Unable to read' $f)"
echo "resets/panics: $(grep -a -c -E 'rst cause|Exception|Panic|wdt' $f)"
grep -a 'Got CO2' $f | sed -E 's/.*CO2=([0-9.]+)ppm temperature=([0-9.-]+).*humidity=([0-9.]+)%.*/\1 \2 \3/' | awk 'NR==1{a=b=$1;c=d=$2;e=g=$3} {if($1<a)a=$1;if($1>b)b=$1;if($2<c)c=$2;if($2>d)d=$2;if($3<e)e=$3;if($3>g)g=$3} END{if(NR)printf "CO2 %.0f-%.0f ppm, T %.2f-%.2f C, RH %.1f-%.1f %%\n",a,b,c,d,e,g}'
echo "first/last: $(grep -a 'Got CO2' $f | head -1 | cut -c1-10) $(grep -a 'Got CO2' $f | tail -1 | cut -c1-10)"
grep -a sclst $f | tail -2
