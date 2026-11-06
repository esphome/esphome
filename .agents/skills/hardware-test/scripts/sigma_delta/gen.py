"""gen.py tpl.yaml > NAME.yaml: the full sigma_delta speaker sequence as the seq script of tpl.yaml.

DC levels, volume, mute, sample formats, stereo, 2 Hz and 50 Hz sines with DUMP lines, CPU while playing, sample
rates, underrun, stop mid-play, finish, rtttl and a play/stop hammer; the run ends with ALLDONE.
"""

import sys

from steps import DUMP, RAW, D, L, M, W, burn, dc, fmt, log, render, stopall

log("idle_before_play")
M("idle_boot")
fmt(16, 1, 44100)
for v, lab in [
    (0, "dc0"),
    (0.5, "dc+50"),
    (-0.5, "dc-50"),
    (1.0, "dc+100"),
    (-1.0, "dc-100"),
    (0.25, "dc+25"),
]:
    dc(v)
    D(400)
    M(f"m16_{lab}")
# volume and mute at +50%
dc(0.5)
for vol in ["0.5", "0.25", "0.0", "1.0"]:
    L(f"id(spk).set_volume({vol});")
    D(400)
    M(f"vol{vol}_dc+50")
L("id(spk).set_mute_state(true);")
D(400)
M("mute_on_dc+50")
L("id(spk).set_mute_state(false);")
D(400)
M("mute_off_dc+50")
# formats
stopall()
for bits, ch in [(8, 1), (24, 1), (32, 1), (16, 2), (32, 2)]:
    fmt(bits, ch, 44100)
    dc(0.5)
    D(400)
    M(f"fmt{bits}b{ch}ch_dc+50")
    stopall()
fmt(16, 2, 44100)
L("sdp::lvl_ = 0.5f; sdp::lvl_r_ = -0.5f; sdp::set_mode(sdp::M_STEREO);")
D(400)
M("st_L+50_R-50")
L("sdp::lvl_ = 0.5f; sdp::lvl_r_ = 0.0f;")
D(400)
M("st_L+50_R0")
stopall()
# sines
fmt(16, 1, 44100)
L("sdp::freq_ = 2; sdp::amp_ = 0.8f; sdp::set_mode(sdp::M_SINE);")
D(500)
M("sine2Hz", 1500)
DUMP("sine2Hz")
L("sdp::freq_ = 50; sdp::amp_ = 0.8f; sdp::set_mode(sdp::M_SINE);")
D(300)
M("sine50Hz", 400)
DUMP("sine50Hz")
# CPU while playing a sine vs idle
L("sdp::freq_ = 440; sdp::set_mode(sdp::M_SINE);")
D(500)
burn("playing44100")
stopall()
D(500)
burn("idle")
# sample rate check through played-frame reports
for rate in [44100, 48000, 22050, 16000, 8000]:
    fmt(16, 1, rate)
    dc(0.25)
    D(1000)
    L("id(f0) = sdp::frames_played_; id(t0) = esp_timer_get_time();")
    D(4000)
    L(
        f'ESP_LOGI("sdp", "RES rate{rate} frames_per_s=%.1f", (sdp::frames_played_ - id(f0)) * 1e6 / (double) (esp_timer_get_time() - id(t0)));'
    )
    stopall()
fmt(16, 1, 44100)
# underrun: 22050 frames (0.5 s) of +50% then nothing, keep sampling across the gap, then resume
log("underrun")
L("sdp::lvl_ = 0.5f; sdp::burst_left_ = 22050; sdp::set_mode(sdp::M_BURST);")
D(200)
M("underrun", 900)
DUMP("underrun")
L('ESP_LOGI("sdp", "RES underrun_state=%d", sdp::st());')
dc(0.5)
D(300)
M("resume_after_underrun")
# stop mid-play: stop feeding and call stop() at once, sample around it
log("stop_mid")
L("sdp::start_capture(300);")
D(100)
L(
    'sdp::set_mode(sdp::M_OFF); id(spk).stop(); id(tstop) = esp_timer_get_time(); ESP_LOGI("sdp", "STOPCALL");'
)
W("sdp::capture_done()", "20s")
L(
    'ESP_LOGI("sdp", "RES stopcall_offset_us=%lld", (long long) (id(tstop) - sdp::cap_t_[0])); sdp::summary("stop_mid"); sdp::dump("stop_mid");'
)
D(500)
M("after_stop")
# finish: fill the ring with +50%, stop feeding, call finish(); should drain then stop
log("finish")
dc(0.5)
D(300)
L("sdp::start_capture(300);")
D(100)
L(
    'sdp::set_mode(sdp::M_OFF); vTaskDelay(25); id(spk).finish(); id(tstop) = esp_timer_get_time(); ESP_LOGI("sdp", "FINISHCALL");'
)
W("sdp::capture_done()", "20s")
L(
    'ESP_LOGI("sdp", "RES finishcall_offset_us=%lld state=%d", (long long) (id(tstop) - sdp::cap_t_[0]), sdp::st()); sdp::summary("finish"); sdp::dump("finish");'
)
# rtttl at 16 kHz through the speaker
log("rtttl")
D(300)
L("id(f0) = sdp::frames_played_; id(t0) = esp_timer_get_time();")
RAW("rtttl.play: 'scale:d=4,o=5,b=100:c,d,e,f,g,a,b,c6'")
D(300)
M("rtttl", 400)
W("!id(tone).is_playing()", "30s")
L(
    'ESP_LOGI("sdp", "RES rtttl frames=%u in %.2fs state=%d", (unsigned) (sdp::frames_played_ - id(f0)), (esp_timer_get_time() - id(t0)) / 1e6, sdp::st());'
)
D(500)
M("after_rtttl")
# rtttl stop mid tune
RAW("rtttl.play: 'long:d=1,o=5,b=60:c,d,e,f'")
D(800)
RAW("rtttl.stop: {id: tone}")
D(500)
L('ESP_LOGI("sdp", "RES rtttl_stop state=%d", sdp::st());')
M("after_rtttl_stop")
# play/stop hammer
log("hammer")
# separate actions, so the speaker loop() runs between each start and stop
RAW("""repeat:
          count: 200
          then:
            - lambda: 'sdp::lvl_ = 0.5f; sdp::set_mode(sdp::M_DC);'
            - delay: !lambda 'return 3 + iteration % 7;'
            - lambda: |-
                sdp::set_mode(sdp::M_OFF);
                if (iteration & 1) id(spk).stop(); else id(spk).finish();
            - delay: !lambda 'return 1 + iteration % 5;'""")
D(1000)
L(
    'ESP_LOGI("sdp", "RES hammer_done state=%d heap=%u", sdp::st(), (unsigned) esp_get_free_heap_size());'
)
M("after_hammer")
L('ESP_LOGI("sdp", "ALLDONE");')

print(render(sys.argv[1]))
