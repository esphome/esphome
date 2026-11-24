# common.sh: sourced by the run*.sh flows. env: TREE, PY, PORT, COORD_IEEE (address -x on the coordinator),
# CHIP (esp32c6 by default, empty for the nRF dongle), SRC (local endpoint for rd and bindrp, 1 by default).
Z=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd); export PY=${PY:-${TREE:?set TREE}/venv/bin/python}; D="$PY $Z/zbdrv.py"
: "${COORD_IEEE:?set COORD_IEEE (address -x on the coordinator)}"
# join_fresh NAME YAML SECS: open the network, flash YAML (log capture for SECS), wait for the join and interrogate
# the device as "NAME boot"; sets short.
join_fresh() {
  $D send "network open -t 254"
  local since; since=$($D pos); $D mark "$1 start"
  CHIP=${CHIP-esp32c6} "$Z/flashdev.sh" "$1" "$2" "$3" || exit 1
  short=$($D waitjoin "$since" 150); echo "joined $short"; [ "$short" = NOJOIN ] && exit 2
  sleep 8
  $D interrogate "$1 boot" "$short"
}
rd() { $D send "zcl send_gen read -d $short --dst-ep $1 -e ${SRC:-1} -c $2 -a ${3:-0x0055}" 2; }  # rd EP CL [ATTR]
wr() { $D send "zcl send_gen write -d $short --dst-ep $1 -e 1 -c $2 -a 0x0055 -t $3 -v $4" 4; }  # wr EP CL TYPE VAL
wf() { wr "$1" 0x000d 0x39 "$($D f32 "$2")"; rd "$1" 0x000d; }  # wf EP FLOAT: write a number, read it back
# bindrp EP CL ATTR TYPE [ARGS]: bind CL to the coordinator and configure reporting of ATTR
bindrp() {
  $D send "zdo bind -c $2 -S $1 -D ${SRC:-1} -s $short -d $COORD_IEEE" 3
  $D send "zcl send_gen config_rp -d $short --dst-ep $1 -e ${SRC:-1} -c $2 -a $3 -t $4 $5" 3
}
