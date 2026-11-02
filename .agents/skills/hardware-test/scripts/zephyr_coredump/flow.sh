#!/bin/zsh
# usage: flow.sh LABEL PORT ELF [OUTDIR]
# Run at least 60 s after the board rebooted from its crash, with the port closed. Proves the stored
# coredump waits for a reader and survives a reader that goes away:
# 1) open the port for 3 s and close it (a reader that leaves mid-print)
# 2) open it for 100 s: the whole dump must print again, then be erased
# 3) open it again for 20 s: dump_config must report no stored dump
# then decode.sh parses the first complete dump and prints the GDB backtrace.
# Needs a python with pyserial (PYTHON, default python3).
D=${0:A:h}
L=$1; P=$2; ELF=$3; S=${4:-.}
PY=${PYTHON:-python3}
CAP=$D/../serial_capture.py
capture() {  # SECONDS OUTFILE: stop the flow when the capture fails (busy or missing port)
  if ! $PY $CAP $P $1 $2 > $2.err 2>&1; then
    echo "capture of $2 failed:"; cat $2.err; exit 1
  fi
}
capture 3 $S/$L-brief.txt
echo "$L brief 3 s open: $(wc -l < $S/$L-brief.txt) lines, CD lines $(grep -a -c '#CD:' $S/$L-brief.txt), END $(grep -a -c '#CD:END#' $S/$L-brief.txt)"
sleep 5
capture 100 $S/$L-full.txt
echo "$L full 100 s open: $(wc -l < $S/$L-full.txt) lines, CD lines $(grep -a -c '#CD:' $S/$L-full.txt), BEGIN $(grep -a -c '#CD:BEGIN#' $S/$L-full.txt), END $(grep -a -c '#CD:END#' $S/$L-full.txt)"
grep -a "crashtest\|zephyr_coredump\|Reset reason\|Has stored\|  Size:\|  Error:\|took a long\|compiled on" $S/$L-full.txt | grep -a -v "#CD:[0-9A-F]" | head -16
sleep 3
capture 20 $S/$L-after.txt
echo "$L reopen 20 s: CD lines $(grep -a -c '#CD:' $S/$L-after.txt); $(grep -a 'Has stored\|  Size:' $S/$L-after.txt | sed -E 's/^\[[^]]*\]//' | tr '\n' ' ')"
echo "== decode"
$D/decode.sh $S/$L-full.txt $ELF $S/$L
