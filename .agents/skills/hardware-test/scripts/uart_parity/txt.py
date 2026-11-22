"""txt.py CAPTURE: the RX bytes of an rd.py capture as text (the RPT report lines).

Exits 1 after the text when PARMRK error marks were found or the port went away (a HUP line), so that text is
not taken for a clean read.
"""

import pathlib
import sys

from uartsim import rx_parse

lines = [
    ln.split(" ", 2)
    for ln in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
]
# unescape once, since a doubled FF can be split across two reads
data, marks = rx_parse(" ".join(p[2] for p in lines if p[1] == "RX"))
sys.stdout.write(data.decode("latin1").replace("\r", ""))
hups = sum(p[1] == "HUP" for p in lines)
if marks or hups:
    sys.exit(f"{marks} PARMRK error marks, {hups} HUP lines")
