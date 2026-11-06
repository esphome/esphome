"""gen2.py NAME tpl.yaml > NAME.yaml: inverted pin levels and CPU cost per sample rate (flash with the pin inverted)."""

import pathlib
import sys

name = sys.argv[1]
steps = []


def L(code):
    steps.append(f"      - lambda: |-\n          {code}")


def D(ms):
    steps.append(f"      - delay: {ms}ms")


def M(label, n=150):
    steps.append(
        f'      - script.execute: {{id: meas, label: "{label}", n: {n}}}\n      - script.wait: meas'
    )


def DUMP(label):
    L(f'sdp::dump("{label}");')


def log(s):
    L(f'ESP_LOGI("sdp", "STEP {s}");')


def fmt(bits, ch, rate):
    L(f"id(spk).set_audio_stream_info(audio::AudioStreamInfo({bits}, {ch}, {rate}));")


def stopall():
    L("sdp::set_mode(sdp::M_OFF);")
    D(60)
    L("id(spk).stop();")
    D(300)


def dc(v):
    L(f"sdp::lvl_ = {float(v)}f; sdp::set_mode(sdp::M_DC);")


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
    L("sdp::burn_start(200);")
    D(400)
    L('ESP_LOGI("sdp", "RES burn_idle iters=%u", (unsigned) sdp::burn_iters_);')
for rate in [44100, 48000, 22050, 8000]:
    fmt(16, 1, rate)
    L("sdp::freq_ = 440; sdp::set_mode(sdp::M_SINE);")
    D(500)
    for _ in range(2):
        L("sdp::burn_start(200);")
        D(400)
        L(f'ESP_LOGI("sdp", "RES burn_{rate} iters=%u", (unsigned) sdp::burn_iters_);')
    stopall()
L('ESP_LOGI("sdp", "ALLDONE");')

tpl = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8")
print(tpl.replace("NAME", name).replace("      # @STEPS@", "\n".join(steps)))
