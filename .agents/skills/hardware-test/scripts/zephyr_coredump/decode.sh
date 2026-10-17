#!/bin/zsh
# usage: decode.sh CAPTURE ELF OUTPREFIX
# Parses the #CD: lines of a serial capture with Zephyr's coredump tools from the sdk-nrf that ESPHome
# installed (SDK_NRF, default: the sdk-nrf directory under the ESPHome cache), serves the dump to GDB on a
# random port and prints the backtrace and the thread list. The ELF is .pioenvs/<name>/zephyr/zephyr/zephyr.elf.
# Every step is checked: a missing tool, a capture without a complete dump, an empty .bin or a gdbserver
# that died all stop the script with exit 1, and the exit status is GDB's otherwise.
CAP=$1; ELF=$2; OUT=$3
SDK_NRF=${SDK_NRF:-$(ls -d ~/Library/Caches/esphome/sdk-nrf ~/.cache/esphome/sdk-nrf 2>/dev/null | head -1)}
[ -n "$SDK_NRF" ] || { echo "sdk-nrf not found, set SDK_NRF"; exit 1; }
F=$(ls -d $SDK_NRF/frameworks/v*/zephyr/scripts/coredump 2>/dev/null | tail -1)
PY=${PYTHON:-$(ls $SDK_NRF/penvs/*/bin/python 2>/dev/null | head -1)}
GDB=$(ls $SDK_NRF/toolchains/*/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb 2>/dev/null | tail -1)
for v in F PY GDB; do
  [ -n "${(P)v}" ] || { echo "$v not found under $SDK_NRF"; exit 1; }
done
[ -f "$ELF" ] || { echo "ELF not found: $ELF"; exit 1; }
# strip capture timestamps and ANSI so the parser sees the raw log lines
sed -E 's/^\[[0-9:.]+\]//; s/\x1b\[[0-9;]*m//g' "$CAP" > $OUT-all.log
# keep only the first complete #CD:BEGIN# .. #CD:END# block
awk '/#CD:BEGIN#/{if(!done){p=1;buf=""}} p{buf=buf $0 "\n"} /#CD:END#/{if(p){printf "%s", buf; done=1; p=0}}' $OUT-all.log > $OUT-raw.log
echo "CD lines in capture: $(grep -c '#CD:' $OUT-all.log) (BEGIN $(grep -c '#CD:BEGIN#' $OUT-all.log), END $(grep -c '#CD:END#' $OUT-all.log)); first complete block: $(grep -c '#CD:' $OUT-raw.log) lines"
[ -s $OUT-raw.log ] || { echo "no complete #CD:BEGIN# .. #CD:END# block in the capture"; exit 1; }
if ! $PY $F/coredump_serial_log_parser.py $OUT-raw.log $OUT.bin > $OUT-parser.log 2>&1; then
  echo "parser failed:"; tail -5 $OUT-parser.log; exit 1
fi
tail -3 $OUT-parser.log
[ -s $OUT.bin ] || { echo "parser wrote an empty $OUT.bin"; exit 1; }
ls -la $OUT.bin | awk '{print "bin bytes", $5}'
PORT=$((20000 + RANDOM % 10000))
$PY $F/coredump_gdbserver.py --port $PORT "$ELF" $OUT.bin > $OUT-gdbserver.log 2>&1 &
G=$!
sleep 3
if ! kill -0 $G 2>/dev/null; then echo "gdbserver exited:"; tail -5 $OUT-gdbserver.log; exit 1; fi
$GDB -q -batch -ex "target remote :$PORT" -ex "bt" -ex "info threads" "$ELF" 2>&1 | grep -v "^warning\|Reading symbols\|^$" | head -40
status=$pipestatus[1]
kill $G 2>/dev/null
exit $status
