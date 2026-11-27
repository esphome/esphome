"""Write the on_client_synced test configs, one per VARIANT, into the current directory.
Usage: gen_configs.py VARIANT...   (p n s d ds; the board is an ESP32-S3 on USB Serial/JTAG, Wi-Fi from secrets)

p   on_client_synced with delayed log lines        n   on_client_synced, no delays
s   p plus deep_sleep: (shutdown sync path)       d   no on_client_synced (dev baseline), delays
ds  d plus deep_sleep:
Every variant has the same `name:` and its own `build_path`, so one Home Assistant entry follows every build. Each
trigger logs `<TRIGGER> direct`, a `cap-now` line from a `[=]` capturing lambda and a `cap-later` line from a 1 s
`set_timeout` with the same capture (a kept StringRef reads garbage there), plus the state_subscription_only check."""

import pathlib
import sys

BASE = """esphome:
  name: client-synced
  build_path: .esphome/build/client-synced-{variant}
  on_shutdown:
    - logger.log: "ON_SHUTDOWN fired"
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  power_save_mode: none
logger:
  level: DEBUG
  hardware_uart: USB_SERIAL_JTAG
ota:
  - platform: esphome
script:
  - id: later_disc
    mode: parallel
    max_runs: 10
    parameters:
      info: string
      addr: string
    then:
      - delay: 2s
      - logger.log:
          format: "DISCONNECTED delayed info=%s addr=%s"
          args: [info.c_str(), addr.c_str()]
switch:
  - platform: template
    name: "Marker"
    optimistic: true
    restore_mode: ALWAYS_ON
text_sensor:
  - platform: template
    name: "Text marker"
    lambda: return {{"marker"}};
    update_interval: never
interval:
  - interval: 60s
    then:
      - logger.log:
          format: "ALIVE uptime=%u free=%u"
          args: ["(unsigned) (millis() / 1000)", "(unsigned) esp_get_free_heap_size()"]
globals:
  - id: g_tag
    type: std::string
    initial_value: '"G"'
  - id: g_count
    type: int
    initial_value: "0"
button:
  - platform: restart
    name: "Restart"
sensor:
"""
SENSOR = """  - platform: template
    name: "Value {i:02d}"
    lambda: return {i}.5f;
    update_interval: never
"""


def cap(tag: str) -> str:
    return f"""    - lambda: |-
        static char keys[16];
        static uint8_t k = 0;
        int local = ++id(g_count);
        auto now_fn = [=]() {{
          ESP_LOGI("cap", "{tag} cap-now g=%s local=%d info=%.*s addr=%.*s", id(g_tag).c_str(), local,
                   (int) client_info.size(), client_info.c_str(), (int) client_address.size(), client_address.c_str());
        }};
        now_fn();
        App.scheduler.set_timeout(&keys[k++ & 15], 1000, [=]() {{
          ESP_LOGI("cap", "{tag} cap-later local=%d info=%.*s addr=%.*s", local,
                   (int) client_info.size(), client_info.c_str(), (int) client_address.size(), client_address.c_str());
        }});
"""


def ss(tag: str) -> str:
    return f"""    - if:
        condition:
          api.connected:
            state_subscription_only: true
        then:
          - logger.log: "{tag} state_subscription_only=true"
        else:
          - logger.log: "{tag} state_subscription_only=false"
"""


def direct(tag: str) -> str:
    return f"""    - logger.log:
        format: "{tag} direct info=%s addr=%s"
        args: [client_info.c_str(), client_address.c_str()]
"""


def delayed(tag: str, d: str = "2s", label: str = "delayed") -> str:
    return f"""    - delay: {d}
    - logger.log:
        format: "{tag} {label} info=%s addr=%s"
        args: [client_info.c_str(), client_address.c_str()]
"""


HA = """    - homeassistant.event:
        event: esphome.client_synced
        data:
          client: !lambda 'return std::string(client_info.c_str());'
    - homeassistant.action:
        action: system_log.write
        data:
          message: !lambda 'return "on_client_synced action for " + std::string(client_info.c_str());'
          level: warning
        on_success:
          - logger.log: "SYNCED action on_success"
        on_error:
          - logger.log: "SYNCED action on_error"
"""
SCRIPT = """    - script.execute:
        id: later_disc
        info: !lambda 'return std::string(client_info.c_str());'
        addr: !lambda 'return std::string(client_address.c_str());'
"""
VARIANTS = ("p", "n", "s", "d", "ds")


def build(variant: str) -> str:
    delay = variant != "n"
    synced = variant in ("p", "n", "s")
    out = BASE.format(variant=variant) + "".join(SENSOR.format(i=i) for i in range(30))
    if variant in ("s", "ds"):
        out += "deep_sleep:\n  id: ds\n"
    out += "api:\n  reboot_timeout: 0s\n  on_client_connected:\n"
    out += direct("CONNECTED") + ss("CONNECTED") + cap("CONNECTED")
    if delay:
        out += delayed("CONNECTED")
    out += (
        "  on_client_disconnected:\n"
        + direct("DISCONNECTED")
        + cap("DISCONNECTED")
        + SCRIPT
    )
    if delay:
        out += delayed("DISCONNECTED", "1s", "delayed1")
    if synced:
        out += (
            "  on_client_synced:\n"
            + direct("SYNCED")
            + ss("SYNCED")
            + cap("SYNCED")
            + HA
        )
        if delay:
            out += delayed("SYNCED")
    return out


for v in sys.argv[1:]:
    if v not in VARIANTS:
        sys.exit(f"unknown variant {v}, pick from {' '.join(VARIANTS)}")
    with pathlib.Path(f"client-synced-{v}.yaml").open("w", encoding="utf-8") as f:
        f.write(build(v))
