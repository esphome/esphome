"""gen.py [OUT]: write OUT (default gen-common.yaml next to this script) with raw frames, buttons, on_pronto trigger,
pronto binary sensors and heap logging; prints each frame's length and the pronto binary sensor data."""

import pathlib
import sys


def frame(n, seed):
    out = []
    for i in range(n):
        if i % 2 == 0:
            out.append(300 + (i * 37 + seed * 101) % 900)
        else:
            out.append(-(400 + (i * 53 + seed * 59) % 1500))
    return out


def pronto(t):
    nums = [0, 4145146 // 38000, (len(t) + 1) // 2, 0]
    for x in t:
        d = x - 20 if x > 0 else -x + 20
        nums.append((d + 13) // 26)
    return " ".join(f"{v:04X}" for v in nums) + " 0181 "


frames = {
    "raw21a": frame(21, 1),
    "raw21b": [x + 260 if x > 0 else x - 260 for x in frame(21, 1)],
    "raw41": frame(41, 2),
    "raw43": frame(43, 3),
    "raw121": frame(121, 4),
    "raw245": frame(245, 5),
}
y = []
y.append("button:")
y.append(
    "  - {platform: template, name: tx nec, on_press: [{logger.log: 'MARK tx nec'}, {remote_transmitter.transmit_nec: {address: 0x1234, command: 0x78}}]}"
)
for k, v in frames.items():
    y.append(
        f"  - platform: template\n    name: tx {k}\n    on_press:\n      - logger.log: 'MARK tx {k}'\n      - remote_transmitter.transmit_raw:\n          code: {v}"
    )
y.append(
    """  - platform: template
    name: soak
    on_press:
      - lambda: id(heap_log).execute("before");
      - repeat:
          count: 100
          then:
            - remote_transmitter.transmit_nec: {{address: 0x1234, command: 0x78}}
            - delay: 150ms
            - remote_transmitter.transmit_raw:
                code: {}
            - delay: 250ms
      - delay: 1s
      - lambda: id(heap_log).execute("after");
""".format(frames["raw245"])
)
y.append(f"""script:
  - id: heap_log
    parameters: {{tag: string}}
    then:
      - lambda: |-
          #ifdef USE_ESP32
          ESP_LOGI("heap", "HEAP %s free=%u largest=%u", tag.c_str(), (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
          #else
          ESP_LOGI("heap", "HEAP %s free=%u largest=%u", tag.c_str(), (unsigned) ESP.getFreeHeap(), (unsigned) ESP.getMaxFreeBlockSize());
          #endif
binary_sensor:
  - platform: remote_receiver
    name: pronto a
    receiver_id: irrx
    pronto:
      data: "{pronto(frames["raw21a"]).strip()}"
    on_press: [{{logger.log: 'BS pronto a ON'}}]
""")
out = (
    pathlib.Path(sys.argv[1])
    if len(sys.argv) > 1
    else pathlib.Path(__file__).with_name("gen-common.yaml")
)
out.write_text("\n".join(y), encoding="utf-8")
for k, v in frames.items():
    print(k, len(v), (4 + len(v)) * 5)
print("PRONTO_A", pronto(frames["raw21a"]))
