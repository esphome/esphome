"""gen2.py tpl.yaml > NAME.yaml: inverted pin levels and CPU cost per sample rate (flash with the pin inverted)."""

import sys

from steps import D, L, M, burn, dc, fmt, log, render, stopall

log("run2")
M("inv_idle")
fmt(16, 1, 44100)
for v, lab in [(0.5, "dc+50"), (-0.5, "dc-50"), (0.0, "dc0")]:
    dc(v)
    D(400)
    M(f"inv_{lab}")
stopall()
D(500)
for _ in range(3):
    burn("idle")
for rate in [44100, 48000, 22050, 8000]:
    fmt(16, 1, rate)
    L("sdp::freq_ = 440; sdp::set_mode(sdp::M_SINE);")
    D(500)
    for _ in range(2):
        burn(rate)
    stopall()
L('ESP_LOGI("sdp", "ALLDONE");')

print(render(sys.argv[1]))
