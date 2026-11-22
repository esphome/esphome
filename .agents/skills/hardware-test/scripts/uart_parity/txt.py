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
if not data:  # wrong port, dead board or wrong baud: not a clean read
    sys.exit("no RX bytes in the capture")
if (
    b"RPT" not in data
):  # the report line this exists to show; garble only means a wrong framing
    sys.exit("no RPT report line in the capture")
short = sum(
    p[1] == "TX" and "SHORT" in p[2] for p in lines
)  # rd.py could not write all of an ack or feed
# a feed that was asked for but never written: the RX buffer probe did not run
hdr = next((p[2] for p in lines if p[1] == "TX" and p[2].startswith("framing=")), "")
if "feed" in hdr.split("options=")[-1].split(",") and not any(
    p[1] == "TX" and p[2].startswith("55") for p in lines
):
    sys.exit("feed was requested but never written")
if marks or hups or short:
    sys.exit(f"{marks} PARMRK error marks, {hups} HUP lines, {short} short writes")
