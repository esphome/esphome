"""txt.py CAPTURE: the RX bytes of an rd.py capture as text (the RPT report lines)."""

import pathlib
import sys

from uartsim import rx_bytes

# unescape once, since a doubled FF can be split across two reads
data = rx_bytes(
    " ".join(
        ln.split(" ", 2)[2]
        for ln in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
        if ln.split(" ", 2)[1] == "RX"
    )
)
sys.stdout.write(data.decode("latin1").replace("\r", ""))
