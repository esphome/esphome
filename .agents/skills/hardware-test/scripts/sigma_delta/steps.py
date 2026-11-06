"""Step helpers shared by gen.py and gen2.py: each call appends one step to STEPS."""

import pathlib

STEPS: list[str] = []


def L(code):
    STEPS.append(f"      - lambda: |-\n          {code}")


def D(ms):
    STEPS.append(f"      - delay: {ms}ms")


def M(label, n=150):
    STEPS.append(
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


def RAW(action):
    STEPS.append(f"      - {action}")


def W(cond, timeout):
    RAW(f"wait_until: {{condition: {{lambda: 'return {cond};'}}, timeout: {timeout}}}")


def burn(label):
    L("sdp::burn_start(200);")
    D(400)
    L(f'ESP_LOGI("sdp", "RES burn_{label} iters=%u", (unsigned) sdp::burn_iters_);')


def render(tpl_path: str) -> str:
    """The template with the collected steps filled in; the name comes from esphome -s name."""
    tpl = pathlib.Path(tpl_path).read_text(encoding="utf-8")
    return tpl.replace("      # @STEPS@", "\n".join(STEPS))
