#!/bin/zsh
# usage: decode.sh CAPTURE ELF OUTPREFIX
# Parses the #CD: lines of a serial capture with Zephyr's coredump tools from the sdk-nrf that ESPHome
# installed (SDK_NRF, default: the sdk-nrf directory under the ESPHome cache), serves the dump to GDB on a
# random port and prints the backtrace and the thread list. The ELF is .pioenvs/<name>/zephyr/zephyr/zephyr.elf.
CAP=$1; ELF=$2; OUT=$3
SDK_NRF=${SDK_NRF:-$(ls -d ~/Library/Caches/esphome/sdk-nrf ~/.cache/esphome/sdk-nrf 2>/dev/null | head -1)}
F=$(ls -d $SDK_NRF/frameworks/v*/zephyr/scripts/coredump | tail -1)
PY=${PYTHON:-$(ls $SDK_NRF/penvs/*/bin/python | head -1)}
GDB=$(ls $SDK_NRF/toolchains/*/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb | tail -1)
# strip capture timestamps and ANSI so the parser sees the raw log lines
sed -E 's/^\[[0-9:.]+\]//; s/\x1b\[[0-9;]*m//g' "$CAP" > $OUT-all.log
# keep only the first complete #CD:BEGIN# .. #CD:END# block
awk '/#CD:BEGIN#/{if(!done){p=1;buf=""}} p{buf=buf $0 "\n"} /#CD:END#/{if(p){printf "%s", buf; done=1; p=0}}' $OUT-all.log > $OUT-raw.log
echo "CD lines in capture: $(grep -c '#CD:' $OUT-all.log) (BEGIN $(grep -c '#CD:BEGIN#' $OUT-all.log), END $(grep -c '#CD:END#' $OUT-all.log)); first complete block: $(grep -c '#CD:' $OUT-raw.log) lines"
$PY $F/coredump_serial_log_parser.py $OUT-raw.log $OUT.bin 2>&1 | tail -3
ls -la $OUT.bin | awk '{print "bin bytes", $5}'
PORT=$((20000 + RANDOM % 10000))
$PY $F/coredump_gdbserver.py --port $PORT "$ELF" $OUT.bin > $OUT-gdbserver.log 2>&1 &
G=$!
sleep 3
$GDB -q -batch -ex "target remote :$PORT" -ex "bt" -ex "info threads" "$ELF" 2>&1 | grep -v "^warning\|Reading symbols\|^$" | head -40
kill $G 2>/dev/null
