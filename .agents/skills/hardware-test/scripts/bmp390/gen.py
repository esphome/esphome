"""Write a BMP390 bench config (ESP32-S3, no Wi-Fi) next to this script.

usage: gen.py NAME [--rate 50Hz] [--iir 128x|off] [--interval 1s] [--fault --kinds NMTWLSHD --probe-delay MS]
              [--reset-at 40,80] [--open-line] [--bus-reset]
Fault kinds, one per event at 30 s and then every 20 s: N next start write NACKs in software, M every start write
NACKs for 5 s, T SDA held low for the next start write, W the same for every transfer for 5 s, L a 5 s continuous
hold, S a 300 ms hold, H and D harness-only held transfers.
"""

import argparse
import pathlib

HERE = pathlib.Path(__file__).parent

FAULT = """
  - interval: 100ms
    then:
      - lambda: |-
          bmpt::fault_tick();
          static constexpr char KIND[] = "KINDS";
          static uint8_t i = 0;
          static uint32_t probe_at = 0;
          uint32_t now = millis();
          if (i < sizeof(KIND) - 1 && now >= 30000u + 20000u * i) {  // events at 30 s, then every 20 s
            char k = KIND[i++];
            ESP_LOGI("bmpt", "FAULT %c at %u failed=%d warn=%d", k, (unsigned) now, id(bmp)->is_failed(),
                     id(bmp)->status_has_warning());
            // S and L hold SDA at once; fault_tick() releases it at the deadline
            if (k == 'S') {
              bmpt::g_fb->hold_until = now + 300;
              bmpt::sda_hold();
            }
            if (k == 'W') bmpt::g_fb->window_until = now + 5000;
            if (k == 'L' || k == 'D') bmpt::g_fb->hold_until = now + 5000;
            if (k == 'L')
              bmpt::sda_hold();
            if (k == 'N') bmpt::g_fb->soft_arm = true;
            if (k == 'M') bmpt::g_fb->soft_until = now + 5000;
            if (k == 'H') {
              uint8_t reg = 0x00, v;
              bmpt::g_fb->arm_start = false;
              bmpt::sda_hold();
              bool hok = bmpt::g_real->write_readv(ADDR, &reg, 1, &v, 1) == esphome::i2c::ERROR_OK;
              bmpt::sda_release();
              ESP_LOGI("bmpt", "HARNESS held transfer ok=%d", hok);
            }
            if (k == 'D') {
              uint8_t reg = 0x00, v;
              for (int n = 0; n < 2; n++)
                bmpt::g_fb->write_readv(ADDR, &reg, 1, &v, 1);
            }
            if (k == 'T') bmpt::g_fb->arm_start = true;
            probe_at = now + PROBEDELAY;
          }
          if (probe_at != 0 && now >= probe_at) {
            probe_at = 0;
            bmpt::probe(ADDR, SCLPIN);
          }
"""

RESET = """
  - interval: 1s
    then:
      - lambda: |-
          static constexpr uint32_t AT[] = {RESETAT};
          static uint8_t i = 0;
          if (i < sizeof(AT) / sizeof(AT[0]) && millis() >= AT[i] * 1000) {
            i++;
            bool ok = bmpt::wr(ADDR, 0x7E, 0xB6);
            delay(10);
            ESP_LOGI("bmpt", "SOFTRESET ok=%d", ok);
            bmpt::dump_regs(ADDR);
            ESP_LOGI("bmpt", "DRDY ok=%d", bmpt::wr(ADDR, 0x19, 0x42));
          }
"""


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--addr", default="0x77")
    ap.add_argument("--rate")
    ap.add_argument("--iir", default="128x")
    ap.add_argument("--interval", default="1s")
    ap.add_argument("--posr", default="8x")
    ap.add_argument("--tosr", default="none")
    ap.add_argument("--fault", action="store_true")
    ap.add_argument("--kinds", default="WTSWTSWT")
    ap.add_argument("--probe-delay", default="7000")
    ap.add_argument(
        "--open-line",
        action="store_true",
        help="fault routes SDA away (floats high) instead of low",
    )
    ap.add_argument(
        "--bus-reset",
        action="store_true",
        help="reset the IDF bus after a failed probe",
    )
    ap.add_argument("--sda", default="1")
    ap.add_argument("--scl", default="2")
    ap.add_argument("--int-pin", default="42")
    ap.add_argument(
        "--no-drdy", action="store_true", help="do not enable data ready on INT"
    )
    ap.add_argument(
        "--reset-at",
        default="",
        help="seconds, comma separated: soft reset the chip behind the driver",
    )
    a = ap.parse_args()

    rate = f"\n    data_rate: {a.rate}" if a.rate else ""
    # data ready on INT, push-pull, active high; the driver's soft reset in setup clears it
    drdy = (
        ""
        if a.no_drdy
        else f'            ESP_LOGI("bmpt", "DRDY ok=%d", bmpt::wr({a.addr}, 0x19, 0x42));\n'
    )
    extra = ""
    if a.fault:
        extra += (
            FAULT.replace("KINDS", a.kinds)
            .replace("PROBEDELAY", a.probe_delay)
            .replace("SCLPIN", a.scl)
        )
    if a.reset_at:
        extra += RESET.replace("RESETAT", a.reset_at)
    yaml = f"""esphome:
  name: {a.name}
  includes:
    - bmp390probe.hpp
  on_boot:
    - priority: 800
      then:
        - lambda: |-
            bmpt::g_real = id(bus);
            bmpt::g_sda = {a.sda};
            bmpt::g_port = id(bus)->get_port();
            static bmpt::FaultBus fb(id(bus));
            id(bmp)->set_i2c_bus(&fb);
            bmpt::g_fb = &fb;
            bmpt::g_bus_reset_on_probe = {"true" if a.bus_reset else "false"};
            bmpt::g_hold_level = {1 if a.open_line else 0};
            ESP_LOGI("bmpt", "PRESETUP");
            bmpt::dump_regs({a.addr});
    - priority: -100
      then:
        - lambda: |-
            ESP_LOGI("bmpt", "POSTSETUP failed=%d", id(bmp)->is_failed());
            bmpt::dump_regs({a.addr});
{drdy}            bmpt::int_counter_begin({a.int_pin});
            bmpt::dump_regs({a.addr});
            bmpt::log_heap();

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

logger:
  level: DEBUG
  hardware_uart: USB_SERIAL_JTAG

i2c:
  id: bus
  sda: GPIO{a.sda}
  scl: GPIO{a.scl}
  scan: true

sensor:
  - platform: bmp3xx_i2c
    id: bmp
    address: {a.addr}
    pressure:
      name: pressure
      oversampling: {a.posr}
      on_value:
        - lambda: 'ESP_LOGI("bmpt", "READ %u %.4f", (unsigned) millis(), x);'
    temperature:
      name: temperature
      oversampling: {a.tosr}
    iir_filter: "{a.iir}"{rate}
    update_interval: {a.interval}

interval:
  - interval: 10s
    then:
      - lambda: |-
          static uint32_t last_edges = 0, last_ms = 0;
          uint32_t e = bmpt::g_int_edges, now = millis();
          ESP_LOGI("bmpt", "INT edges=%u in %u ms failed=%d warn=%d xfers=%u fails=%u starts=%u "
                   "start_fails=%u normal_writes=%u",
                   (unsigned) (e - last_edges), (unsigned) (now - last_ms), id(bmp)->is_failed(),
                   id(bmp)->status_has_warning(), bmpt::g_fb->xfers, bmpt::g_fb->fails, bmpt::g_fb->start_writes,
                   bmpt::g_fb->start_fails, bmpt::g_fb->normal_writes);
          last_edges = e;
          last_ms = now;
          if (!bmpt::g_held) bmpt::dump_regs({a.addr});
          bmpt::log_heap();
{extra.replace("ADDR", a.addr)}"""
    out = HERE / f"{a.name}.yaml"
    out.write_text(yaml, encoding="utf-8")
    print(out)


if __name__ == "__main__":
    main()
