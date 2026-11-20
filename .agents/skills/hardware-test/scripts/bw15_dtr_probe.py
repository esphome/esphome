"""bw15_dtr_probe.py PORT DELAY TOTAL OUT: boot the BW15 kit with DTR released, assert DTR after DELAY seconds.

The kit's CH340 drives PA00 from DTR; a released DTR holds PA00 low. Pulses RTS (CEN) once, then logs every
line with its time since the reset to OUT for TOTAL seconds.
"""

import pathlib
import sys
import time

import serial

port, delay, total, out = (
    sys.argv[1],
    float(sys.argv[2]),
    float(sys.argv[3]),
    sys.argv[4],
)
s = serial.Serial(port=None, baudrate=115200, timeout=0.2)
s.port = port
s.dtr = False
s.rts = False
s.open()
s.rts = True
time.sleep(0.1)
s.rts = False
s.reset_input_buffer()
t0 = time.time()
flipped = False
with pathlib.Path(out).open("w", encoding="utf-8") as f:
    while time.time() - t0 < total:
        if not flipped and time.time() - t0 >= delay:
            s.dtr = True
            flipped = True
            f.write(f"[{time.time() - t0:6.2f}] [probe] DTR asserted\n")
            f.flush()
        line = s.readline()
        if line:
            text = line.decode("utf-8", "replace").rstrip()
            f.write(f"[{time.time() - t0:6.2f}] {text}\n")
            f.flush()
s.dtr = False
s.close()
