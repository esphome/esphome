"""Write the two-radio air test configs for esphome#21110: gen_pair.py OUTDIR.

lpair: two SX127x in LoRa; fpair: two SX127x in FSK (63 byte packets); xpair: SX127x (A) and SX126x (B) in LoRa.
Each has raw links, reliability links and session plus rolling code links, with entity_transport both ways, run by
sxlink.hpp from a 20 ms interval. Only the interrupt pads (GPIO4 for A, GPIO22 for B) are driven; the SPI, CS, reset and
busy pins are swapped for fake pins at on_boot 1100 and never set up.
"""

from pathlib import Path
import sys

SX127X_LORA = """\
  frequency: 917MHz
  modulation: LORA
  bandwidth: 125_0kHz
  spreading_factor: 7
  coding_rate: CR_4_5
  preamble_size: 8
  crc_enable: true
  rx_start: true"""

SX127X_FSK = """\
  frequency: 917MHz
  modulation: FSK
  bitrate: 9600
  deviation: 5kHz
  bandwidth: 25_0kHz
  bitsync: true
  packet_mode: true
  sync_value: [0x33, 0x33]
  preamble_size: 4
  preamble_detect: 2
  crc_enable: true
  rx_start: true"""

SX126X_LORA = """\
  hw_version: sx1262
  rf_switch: true
  frequency: 917MHz
  modulation: LORA
  bandwidth: 125_0kHz
  spreading_factor: 7
  coding_rate: CR_4_5
  preamble_size: 8
  crc_enable: true
  rx_start: true"""


def pin(num: int, extra: str = "", indent: int = 4) -> str:
    pad = " " * indent
    return f"\n{pad}number: GPIO{num}\n{pad}allow_other_uses: true{extra}"


def radio127(rid: str, bus: str, dio: int, body: str) -> str:
    return f"""\
  - id: {rid}
    spi_id: {bus}
    cs_pin:{pin(5).replace(chr(10) + "    ", chr(10) + "      ")}
    rst_pin:{pin(21).replace(chr(10) + "    ", chr(10) + "      ")}
    dio0_pin:
      number: GPIO{dio}
      mode:
        input: true
        pulldown: true
{chr(10).join("  " + line for line in body.splitlines())}
    on_packet:
      then:
        - lambda: 'sxlink::DRV[{0 if rid == "radio_a" else 1}]++;'
"""


def radio126(rid: str, bus: str, dio: int, body: str) -> str:
    return f"""\
  - id: {rid}
    spi_id: {bus}
    cs_pin:{pin(5).replace(chr(10) + "    ", chr(10) + "      ")}
    rst_pin:{pin(21).replace(chr(10) + "    ", chr(10) + "      ")}
    busy_pin: GPIO36
    dio1_pin:
      number: GPIO{dio}
      mode:
        input: true
        pulldown: true
{chr(10).join("  " + line for line in body.splitlines())}
    on_packet:
      then:
        - lambda: 'sxlink::DRV[{0 if rid == "radio_a" else 1}]++;'
"""


def link(lid: str, plat: str, key: str, radio: str, size: int, layers: str = "") -> str:
    out = f"  - platform: {plat}\n    id: {lid}\n    {key}: {radio}\n    max_packet_size: {size}\n"
    return out + layers


def layers(addr: int, dest: int, session: bool) -> str:
    out = f"""\
    addressing:
      address: {addr}
      destination: {dest}
"""
    if session:
        out += """\
    encryption:
      key: Johnny Cash
      rolling_code_enable: true
"""
    out += """\
    reliability:
      ack_delay: 100ms
"""
    if session:
        out += f"""\
    session:
      binary_sensor:
        name: session {addr}
"""
    return out


def config(name: str) -> str:
    fsk = name.startswith("fpair")
    mixed = name.startswith("xpair")
    size = 63 if fsk else 255
    body_a = SX127X_FSK if fsk else SX127X_LORA
    b_plat, b_key = ("sx126x", "sx126x_id") if mixed else ("sx127x", "sx127x_id")
    radios = "sx127x:\n" + radio127("radio_a", "bus_a", 4, body_a)
    if mixed:
        radios += "sx126x:\n" + radio126("radio_b", "bus_b", 22, SX126X_LORA)
        b_model, b_install = (
            "sxair::Sx126x",
            'install_sx126x(id(bus_b), id(radio_b), &sxpair::MB, &sxpair::PB, &sxpair::RSTB, "B", 22)',
        )
        b_class = "SX126x"
        b_ns, b_err = "sx126x", "sx126x::SX126xError::SX126X_ERROR_NONE"
        b_listener = "sx126x::SX126xTransmitListener"
        b_errtype = "sx126x::SX126xError"
        hdr = '#include "esphome/components/sx126x/sx126x.h"'
    else:
        radios += radio127("radio_b", "bus_b", 22, body_a)
        b_model, b_install = (
            "sxair::Sx127x",
            'install_sx127x(id(bus_b), id(radio_b), &sxpair::MB, &sxpair::PB, "B", 22)',
        )
        b_class = "SX127x"
        b_ns, b_err = "sx127x", "sx127x::SX127xError::SX127X_ERROR_NONE"
        b_listener = "sx127x::SX127xTransmitListener"
        b_errtype = "sx127x::SX127xError"
        hdr = ""
    links = "packet_interface:\n"
    links += link("raw_a", "sx127x", "sx127x_id", "radio_a", size)
    links += link("raw_b", b_plat, b_key, "radio_b", size)
    links += link("rel_a", "sx127x", "sx127x_id", "radio_a", size, layers(1, 2, False))
    links += link("rel_b", b_plat, b_key, "radio_b", size, layers(2, 1, False))
    links += link("ses_a", "sx127x", "sx127x_id", "radio_a", size, layers(3, 4, True))
    links += link("ses_b", b_plat, b_key, "radio_b", size, layers(4, 3, True))
    glue = f"""\
#pragma once
#include "sxlink.hpp"
#include "esphome/components/sx127x/sx127x.h"
{hdr}
namespace sxpair {{
using namespace esphome;
static sxair::Sx127x MA;
static {b_model} MB;
static sxair::PinSet PA, PB;
static sxair::Sx126xRst RSTB;
struct TxL : {b_listener} {{
  void on_transmit_complete({b_errtype} r) override {{ ESP_LOGI("ev", "EV peer async done result=%d", (int) r); }}
}} TL;
static {b_ns}::{b_class} *RB = nullptr;
inline bool b_async(const uint8_t *d, size_t n) {{ return RB->start_transmit_async(&TL, d, n) == {b_err}; }}
}}  // namespace sxpair
"""
    defines = "-DSXL_FSK" if fsk else "-DSXL_LORA"
    yaml = f"""\
# esphome#21110 air test ({name}), generated by gen_pair.py
esphome:
  name: {name}
  includes: [sxair.hpp, sxlink.hpp, {name}_glue.h]
  platformio_options:
    build_flags: {defines}
  on_boot:
    - priority: 1100
      then:
        - lambda: |-
            sxair::install_sx127x(id(bus_a), id(radio_a), &sxpair::MA, &sxpair::PA, "A", 4);
            sxair::{b_install};
            sxpair::RB = id(radio_b);
            auto &c = sxlink::C;
            c.raw_a = id(raw_a); c.raw_b = id(raw_b);
            c.links[0] = id(raw_a); c.links[1] = id(raw_b); c.links[2] = id(rel_a);
            c.links[3] = id(rel_b); c.links[4] = id(ses_a); c.links[5] = id(ses_b); c.nlinks = 6;
            c.ma = &sxpair::MA; c.mb = &sxpair::MB;
            c.sa = id(sens_a); c.sb = id(sens_b); c.sa2 = id(sens_a2); c.sb2 = id(sens_b2);
            c.ra_on_b = id(rem_a_on_b); c.rb_on_a = id(rem_b_on_a);
            c.ra2_on_b = id(rem_a2_on_b); c.rb2_on_a = id(rem_b2_on_a);
            c.b_async = sxpair::b_async;
            sxlink::RXA.attach(id(raw_a));
            sxlink::RXB.attach(id(raw_b));
            sxlink::next_ms = 8000;
    - priority: 300
      then:
        - lambda: sxair::late_pins();
esp32:
  board: pico32
  framework:
    type: esp-idf
logger:
  level: VERBOSE
  tx_buffer_size: 1024
  logs:
    component: DEBUG
    scheduler: DEBUG
    sensor: DEBUG
    spi_device: DEBUG
    sx127x: DEBUG
    sx126x: DEBUG
    preferences: DEBUG
spi:
  - id: bus_a
    interface: software
    clk_pin:{pin(18, indent=6)}
    mosi_pin:{pin(23, indent=6)}
    miso_pin:{pin(19, indent=6)}
  - id: bus_b
    interface: software
    clk_pin:{pin(18, indent=6)}
    mosi_pin:{pin(23, indent=6)}
    miso_pin:{pin(19, indent=6)}
{radios}{links}
entity_transport:
  - id: et_a
    packet_interface: rel_a
    sensors: [sens_a]
  - id: et_b
    packet_interface: rel_b
    sensors: [sens_b]
  - id: et_a2
    packet_interface: ses_a
    sensors: [sens_a2]
  - id: et_b2
    packet_interface: ses_b
    sensors: [sens_b2]
sensor:
"""
    for sid in ("sens_a", "sens_b", "sens_a2", "sens_b2"):
        yaml += f"""\
  - platform: template
    id: {sid}
    update_interval: never
"""
    for rid, et, remote in (
        ("rem_a_on_b", "et_b", "sens_a"),
        ("rem_b_on_a", "et_a", "sens_b"),
        ("rem_a2_on_b", "et_b2", "sens_a2"),
        ("rem_b2_on_a", "et_a2", "sens_b2"),
    ):
        yaml += f"""\
  - platform: entity_transport
    id: {rid}
    entity_transport_id: {et}
    provider: {name}
    remote_id: {remote}
    on_value:
      - lambda: 'ESP_LOGI("ev", "EV remote {rid} = %.1f", x);'
"""
    yaml += """\
interval:
  - interval: 20ms
    then:
      - lambda: sxlink::run();
"""
    return yaml, glue


def main() -> None:
    out = Path(sys.argv[1])
    for name in ("lpair", "fpair", "xpair"):
        yaml, glue = config(name)
        (out / f"{name}.yaml").write_text(yaml)
        (out / f"{name}_glue.h").write_text(glue)


if __name__ == "__main__":
    main()
