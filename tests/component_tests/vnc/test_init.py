"""Code generation and validation tests for the vnc component."""

from collections.abc import Callable
import logging
from pathlib import Path
import re

import pytest

from esphome.config import read_config
from esphome.core import CORE

TEST_CARD = "show_test_card()"
UPDATE_NEVER = "set_update_interval(4294967295UL)"

BASE = """
esphome:
  name: vnc-test

esp32:
  board: esp32dev

logger:

wifi:
  ssid: MySSID
  password: password1

spi:
  clk_pin: GPIO14
  mosi_pin: GPIO12
  miso_pin: GPIO13
"""

IT8951 = """
display:
  - platform: it8951
    id: epd
    model: m5stack-m5paper
"""

VNC = """
vnc:
  - host: 192.168.1.20
"""

EPAPER_SPI = """
  - platform: epaper_spi
    id: other
    model: ssd1677
    dimensions:
      width: 200
      height: 200
    cs_pin: GPIO5
    dc_pin: GPIO21
    busy_pin: GPIO22
    reset_pin: GPIO25
"""

TOUCHSCREEN = """
i2c:
  sda: GPIO32
  scl: GPIO33

touchscreen:
  - platform: gt911
    id: touch
    display: {display}
"""


S3_BASE = """
esphome:
  name: vnc-test

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

psram:
  mode: octal
  speed: 80MHz

wifi:
  ssid: MySSID
  password: password1
"""

RPI_DPI_RGB = """
display:
  - platform: rpi_dpi_rgb
    id: rgb
    dimensions:
      width: 800
      height: 480
    de_pin: 40
    hsync_pin: 39
    vsync_pin: 41
    pclk_pin: 42
    data_pins:
      red: [45, 48, 47, 21, 14]
      green: [5, 6, 7, 15, 16, 4]
      blue: [8, 3, 46, 9, 1]
"""


@pytest.fixture(autouse=True)
def _clear_toolchain() -> None:
    # A host build earlier in the same session leaves the host toolchain selected.
    CORE.toolchain = None


@pytest.fixture
def write_yaml(tmp_path: Path) -> Callable[..., Path]:
    def writer(*parts: str, base: str = BASE) -> Path:
        path = tmp_path / "vnc.yaml"
        path.write_text(base + "".join(parts), encoding="utf-8")
        return path

    return writer


@pytest.fixture
def config_error(
    write_yaml: Callable[..., Path], capsys: pytest.CaptureFixture[str]
) -> Callable[..., str]:
    """Validate a config that must fail and return the printed errors."""

    def validate(*parts: str, base: str = BASE) -> str:
        CORE.config_path = write_yaml(*parts, base=base)
        assert read_config({}) is None
        return capsys.readouterr().out

    return validate


def _code_lines(main_cpp: str) -> str:
    """main.cpp without the config dump comments, which never reach the firmware."""
    return "\n".join(
        line for line in main_cpp.splitlines() if not line.lstrip().startswith("//")
    )


def _auth_key(main_cpp: str) -> list[int]:
    match = re.search(
        r"static constexpr uint8_t \w+_auth_key\[\] PROGMEM = \{([^}]*)\};", main_cpp
    )
    assert match is not None
    return [int(byte) for byte in match.group(1).split(",")]


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_it8951_owned_by_vnc(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(write_yaml(IT8951, VNC))

    assert TEST_CARD not in main_cpp
    assert f"epd->{UPDATE_NEVER}" in main_cpp
    assert re.search(r"new\(\w+\) vnc::VncClient\(epd\);", main_cpp)
    assert 'set_host("192.168.1.20");' in main_cpp
    assert "set_port(5900);" in main_cpp
    assert "set_reconnect_interval(5000);" in main_cpp
    assert "set_update_delay(200);" in main_cpp
    assert "set_max_update_delay(1000);" in main_cpp
    assert "set_pixel_format(vnc::VncPixelFormat::VNC_PIXEL_FORMAT_AUTO);" in main_cpp


def test_epaper_spi_owned_by_vnc(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(
        write_yaml("display:", EPAPER_SPI, "\nvnc:\n  - host: 192.168.1.20\n")
    )

    assert TEST_CARD not in main_cpp
    assert f"other->{UPDATE_NEVER}" in main_cpp
    assert re.search(r"new\(\w+\) vnc::VncClient\(other\);", main_cpp)


def test_display_with_generated_id(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(
        write_yaml("display:\n  - platform: it8951\n    model: m5stack-m5paper\n", VNC)
    )

    assert TEST_CARD not in main_cpp
    assert UPDATE_NEVER in main_cpp
    assert "vnc::VncClient(" in main_cpp


def test_only_the_vnc_display_loses_test_card(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(
        write_yaml(
            IT8951, EPAPER_SPI, "\nvnc:\n  - host: 192.168.1.20\n    display_id: epd\n"
        )
    )

    assert f"other->{TEST_CARD}" in main_cpp
    assert f"epd->{TEST_CARD}" not in main_cpp
    assert f"epd->{UPDATE_NEVER}" in main_cpp


def test_options(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(
        write_yaml(
            IT8951,
            """
vnc:
  - host: vnc.local
    port: 5901
    reconnect_interval: 2s
    update_delay: 0ms
    max_update_delay: 3s
    pixel_format: RGB332
    on_connect:
      - logger.log: up
    on_disconnect:
      - logger.log: down
""",
        )
    )

    assert 'set_host("vnc.local");' in main_cpp
    assert "set_port(5901);" in main_cpp
    assert "set_reconnect_interval(2000);" in main_cpp
    assert "set_update_delay(0);" in main_cpp
    assert "set_max_update_delay(3000);" in main_cpp
    assert "set_pixel_format(vnc::VncPixelFormat::VNC_PIXEL_FORMAT_RGB332);" in main_cpp
    assert "add_on_connect_callback(" in main_cpp
    assert "add_on_disconnect_callback(" in main_cpp


def test_two_clients_on_two_displays(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(
        write_yaml(
            IT8951,
            EPAPER_SPI,
            """
vnc:
  - host: 192.168.1.20
    display_id: epd
  - host: 192.168.1.21
    display_id: other
""",
        )
    )

    assert re.search(r"vnc::VncClient\(epd\);", main_cpp)
    assert re.search(r"vnc::VncClient\(other\);", main_cpp)
    assert TEST_CARD not in main_cpp


def test_no_password_no_auth(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(write_yaml(IT8951, VNC))

    assert "USE_VNC_AUTH" not in _defines()
    assert "auth_key" not in main_cpp
    assert "USE_VNC_TOUCHSCREEN" not in _defines()
    assert "register_touchscreen" not in main_cpp


@pytest.mark.parametrize(
    ("password", "key"),
    [
        ("secret12", [0xCE, 0xA6, 0xC6, 0x4E, 0xA6, 0x2E, 0x8C, 0x4C]),
        ("Zq9", [0x5A, 0x8E, 0x9C, 0, 0, 0, 0, 0]),
        ("secret1234", [0xCE, 0xA6, 0xC6, 0x4E, 0xA6, 0x2E, 0x8C, 0x4C]),
    ],
)
def test_password_emits_bit_reversed_key(
    password: str,
    key: list[int],
    generate_main: Callable[[str | Path], str],
    write_yaml: Callable[..., Path],
) -> None:
    main_cpp = generate_main(
        write_yaml(
            IT8951, f"\nvnc:\n  - host: 192.168.1.20\n    password: {password}\n"
        )
    )

    assert "USE_VNC_AUTH" in _defines()
    assert _auth_key(main_cpp) == key
    assert re.search(r"->set_auth_key\(\w+_auth_key\);", main_cpp)
    code = _code_lines(main_cpp)
    assert password not in code
    assert password[:8] not in code


def test_long_password_warns(
    generate_main: Callable[[str | Path], str],
    write_yaml: Callable[..., Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        generate_main(
            write_yaml(
                IT8951, "\nvnc:\n  - host: 192.168.1.20\n    password: secret123\n"
            )
        )

    assert caplog.text.count("only the first 8 are used") == 1


def test_touchscreen_forwarding(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(
        write_yaml(
            IT8951,
            TOUCHSCREEN.format(display="epd"),
            "\nvnc:\n  - host: 192.168.1.20\n    touchscreen_id: touch\n",
        )
    )

    assert "USE_VNC_TOUCHSCREEN" in _defines()
    assert re.search(r"->register_touchscreen\(touch\);", main_cpp)


def test_action_codegen(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[..., Path]
) -> None:
    main_cpp = generate_main(
        write_yaml(
            IT8951,
            """
vnc:
  - id: remote
    host: 192.168.1.20

interval:
  - interval: 60s
    then:
      - vnc.send_pointer:
          x: 10
          y: 20
          buttons: [left, wheel_down]
      - vnc.send_key: Return
      - vnc.send_key:
          key: "A"
          action: down
      - vnc.send_key: 7
      - vnc.type_text: "é€"
      - vnc.refresh
""",
        )
    )

    assert "remote->send_pointer(10, 20, 17);" in main_cpp
    assert (
        "remote->send_key(0xFF0D, vnc::VncKeyAction::VNC_KEY_ACTION_PRESS);" in main_cpp
    )
    assert "remote->send_key(0x41, vnc::VncKeyAction::VNC_KEY_ACTION_DOWN);" in main_cpp
    assert (
        "remote->send_key(0x37, vnc::VncKeyAction::VNC_KEY_ACTION_PRESS);" in main_cpp
    )
    assert r'remote->type_text("\303\251\342\202\254", 5);' in main_cpp
    assert "remote->refresh();" in main_cpp


@pytest.mark.parametrize(
    ("display_options", "error"),
    [
        pytest.param(
            "    lambda: |-\n      it.fill(Color::BLACK);\n",
            "vnc draws the whole of display 'epd'",
            id="lambda",
        ),
        pytest.param(
            "    pages:\n      - id: page1\n        lambda: |-\n"
            "          it.fill(Color::BLACK);\n",
            "vnc draws the whole of display 'epd'",
            id="pages",
        ),
        pytest.param(
            "    show_test_card: true\n",
            "vnc draws the whole of display 'epd'",
            id="show_test_card",
        ),
        pytest.param(
            "    auto_clear_enabled: true\n",
            "vnc draws the whole of display 'epd'",
            id="auto_clear_enabled",
        ),
    ],
)
def test_display_with_own_writer_rejected(
    display_options: str, error: str, config_error: Callable[..., str]
) -> None:
    assert error in config_error(IT8951, display_options, VNC)


def test_lvgl_on_same_display_rejected(config_error: Callable[..., str]) -> None:
    out = config_error(IT8951, VNC, "\nlvgl:\n  displays: [epd]\n")

    assert "Display 'epd' is also used by LVGL" in out


def test_lvgl_on_other_display_accepted(
    write_yaml: Callable[..., Path],
) -> None:
    CORE.config_path = write_yaml(
        IT8951,
        EPAPER_SPI,
        "\nvnc:\n  - host: 192.168.1.20\n    display_id: epd\n",
        "\nlvgl:\n  displays: [other]\n",
    )

    assert read_config({}) is not None


def test_two_clients_on_one_display_rejected(
    config_error: Callable[..., str],
) -> None:
    out = config_error(
        IT8951, "\nvnc:\n  - host: 192.168.1.20\n  - host: 192.168.1.21\n"
    )

    assert "Display 'epd' is drawn by more than one component: vnc, vnc" in out


def test_mipi_spi_rejected(config_error: Callable[..., str]) -> None:
    out = config_error(
        """
display:
  - platform: mipi_spi
    id: epd
    model: st7735
    cs_pin: GPIO15
    dc_pin: GPIO27
    reset_pin: GPIO26
""",
        VNC,
    )

    assert "Display 'epd' uses the mipi_spi platform" in out


def test_rotated_rpi_dpi_rgb_rejected(config_error: Callable[..., str]) -> None:
    out = config_error(RPI_DPI_RGB, "    rotation: 90\n", VNC, base=S3_BASE)

    assert "Display 'rgb' uses the rpi_dpi_rgb platform, which ignores rotation" in out


def test_unrotated_rpi_dpi_rgb_accepted(write_yaml: Callable[..., Path]) -> None:
    CORE.config_path = write_yaml(RPI_DPI_RGB, VNC, base=S3_BASE)

    assert read_config({}) is not None


def test_waveshare_epaper_rejected(config_error: Callable[..., str]) -> None:
    out = config_error(
        """
display:
  - platform: waveshare_epaper
    id: epd
    model: 1.54in
    cs_pin: GPIO15
    dc_pin: GPIO27
    busy_pin: GPIO26
    reset_pin: GPIO25
""",
        VNC,
    )

    assert "Display 'epd' uses the waveshare_epaper platform" in out
    assert "use epaper_spi" in out


def test_touchscreen_on_other_display_rejected(
    config_error: Callable[..., str],
) -> None:
    out = config_error(
        IT8951,
        EPAPER_SPI,
        TOUCHSCREEN.format(display="other"),
        "\nvnc:\n  - host: 192.168.1.20\n    display_id: epd\n"
        "    touchscreen_id: touch\n",
    )

    assert "Touchscreen 'touch' belongs to display 'other', not 'epd'" in out


@pytest.mark.parametrize(
    ("vnc_options", "error"),
    [
        pytest.param(
            '    password: ""\n', "The password must not be empty", id="empty"
        ),
        pytest.param(
            "    reconnect_interval: 500ms\n",
            "value must be at least 1s",
            id="reconnect",
        ),
        pytest.param("    port: 0\n", "value must be at least 1", id="port"),
        pytest.param(
            "    pixel_format: rgb888\n", "Unknown value 'rgb888'", id="format"
        ),
    ],
)
def test_option_rejected(
    vnc_options: str, error: str, config_error: Callable[..., str]
) -> None:
    out = config_error(IT8951, VNC, vnc_options)

    assert error in out


@pytest.mark.parametrize("host", ["::1", "fe80::1", "2001:db8::10"])
def test_ipv6_host_rejected(host: str, config_error: Callable[..., str]) -> None:
    out = config_error(IT8951, f'\nvnc:\n  - host: "{host}"\n')

    assert "IPv6 addresses are not supported" in out
