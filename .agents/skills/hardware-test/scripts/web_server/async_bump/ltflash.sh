# Sourced by run.sh and bkab.sh. ltflash bk|bw PORT UF2 LOG: flash a LibreTiny kit with the Python and the LibreTiny
# platform from ./pio (else ~/.platformio); BK72xx gets three tries.
ltflash() {
  local d s py lt tool tries=1 to=600 opt=()
  d=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd); s=$(dirname "$(dirname "$d")")
  py=$d/pio/penv/.libretiny/bin/python; [ -x "$py" ] || py=~/.platformio/penv/.libretiny/bin/python
  lt=$(ls -d "$d"/pio/platforms/libretiny* 2> /dev/null | head -1); [ -n "$lt" ] && opt=(-L "$lt")
  case $1 in
    bk) tool=flash_bk72xx.py; tries=3; to=400 ;;
    bw) tool=flash_rtl87xx_kit.py ;;
    *) echo "ltflash: unknown kind $1"; return 1 ;;
  esac
  for _ in $(seq 1 $tries); do
    timeout $to "$py" "$s/$tool" "${opt[@]}" flash write -d "$2" "$3" > "$4" 2>&1 && return 0
  done
  return 1
}
