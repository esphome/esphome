"""Tests for the AP+STA coexistence (``coexist:``) and NAPT (``napt:``) options.

Both options live in the ``wifi:`` ``ap:`` block and are ESP32/ESP-IDF only;
``coexist:`` keeps the access point up while the station stays connected, and
``napt:`` routes the AP subnet out through the station.
"""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.core import CORE
from tests.component_tests.wifi import sdkconfig_option

_AP_DEFINE = "USE_WIFI_AP"
_APSTA_DEFINE = "USE_WIFI_APSTA"
_NAPT_DEFINE = "USE_WIFI_AP_NAPT"

_ESP32_IDF = "esp32:\n  board: esp32dev\n  framework:\n    type: esp-idf\n"
_ESP8266 = "esp8266:\n  board: d1_mini\n"
_STATION = "  ssid: test\n  password: testtest\n"


def _defines() -> set[str]:
    """Names of every define the config generated."""
    return {define.name for define in CORE.defines}


def _write_config(tmp_path: Path, platform: str, wifi: str) -> Path:
    """Write a throwaway config so each case can pick its own combination."""
    config = tmp_path / "wifi.yaml"
    config.write_text(
        f"esphome:\n  name: test\n{platform}wifi:\n{wifi}", encoding="utf-8"
    )
    return config


@pytest.mark.parametrize("config_file", ["coexist.yaml", "coexist_legacy_ssid.yaml"])
def test_coexist_emits_apsta_without_napt(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """``coexist:`` keeps the AP up through WIFI_MODE_APSTA and no NAPT.

    Both the ``networks:`` list and the legacy single ``ssid:`` spelling count
    as a station for the option.
    """
    main_cpp = generate_main(component_config_path(config_file))
    defines = _defines()

    assert "set_ap_coexist(true);" in main_cpp
    assert "set_ap_napt(" not in main_cpp
    assert _AP_DEFINE in defines
    assert _APSTA_DEFINE in defines
    assert _NAPT_DEFINE not in defines
    # Roaming scans after a connect would take the AP down again.
    assert "set_post_connect_roaming(false);" in main_cpp
    assert sdkconfig_option("CONFIG_LWIP_IP_FORWARD") is None
    assert sdkconfig_option("CONFIG_LWIP_IPV4_NAPT") is None


def test_napt_emits_ip_forwarding(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """``napt:`` turns on lwip forwarding and the driver's NAPT hook."""
    main_cpp = generate_main(component_config_path("coexist_napt.yaml"))
    defines = _defines()

    assert "set_ap_coexist(true);" in main_cpp
    assert "set_ap_napt(true);" in main_cpp
    assert _APSTA_DEFINE in defines
    assert _NAPT_DEFINE in defines
    assert sdkconfig_option("CONFIG_LWIP_IP_FORWARD") is True
    assert sdkconfig_option("CONFIG_LWIP_IPV4_NAPT") is True


def test_ap_defaults_stay_plain(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without the options the AP stays a plain fallback access point."""
    main_cpp = generate_main(component_config_path("ap_default.yaml"))
    defines = _defines()

    assert "set_ap_coexist(" not in main_cpp
    assert "set_ap_napt(" not in main_cpp
    assert _AP_DEFINE in defines
    assert _APSTA_DEFINE not in defines
    assert _NAPT_DEFINE not in defines
    # post_connect_roaming defaults to true in C++, so no setter is emitted.
    assert "set_post_connect_roaming(" not in main_cpp
    assert sdkconfig_option("CONFIG_LWIP_IP_FORWARD") is None
    assert sdkconfig_option("CONFIG_LWIP_IPV4_NAPT") is None
    # An AP is configured, so the esp32 reconciler keeps SoftAP and the DHCP
    # server in the build instead of trimming them.
    assert sdkconfig_option("CONFIG_ESP_WIFI_SOFTAP_SUPPORT") is None
    assert sdkconfig_option("CONFIG_LWIP_DHCPS") is None


# The codegen pass stops at the first failure, so the coroutines queued behind
# it are never awaited; core does the same for its pipeline tests.
@pytest.mark.filterwarnings("ignore::RuntimeWarning")
@pytest.mark.parametrize(
    ("platform", "wifi", "match"),
    [
        # APSTA lives in the esp-idf driver only.
        (
            _ESP8266,
            f"{_STATION}  ap:\n    ssid: fallback\n    coexist: true\n",
            "only supported on ESP32",
        ),
        # Coexistence needs a station for the AP to fall back from.
        (
            _ESP32_IDF,
            "  post_connect_roaming: false\n"
            "  ap:\n    ssid: fallback\n    coexist: true\n",
            "requires at least one STA network",
        ),
        # The default post_connect_roaming scan would drop the AP again.
        (
            _ESP32_IDF,
            f"{_STATION}  ap:\n    ssid: fallback\n    coexist: true\n",
            "incompatible with post_connect_roaming",
        ),
        # NAPT only makes sense on top of coexistence.
        (
            _ESP32_IDF,
            f"{_STATION}  ap:\n    ssid: fallback\n    napt: true\n",
            r"requires AP\+STA coexistence",
        ),
    ],
)
def test_invalid_combinations(
    tmp_path: Path,
    generate_main: Callable[[str | Path], str],
    platform: str,
    wifi: str,
    match: str,
) -> None:
    """Unsupported combinations are rejected while the code is generated.

    These checks live in ``to_code()``, so they surface on the codegen pass
    rather than during config validation. Two further branches are unreachable
    through a config file: ``napt:`` on another platform (``coexist:`` reports
    first) and the legacy ``ssid:`` spelling of the station lookup, which
    validation has already folded into ``networks`` by then.
    """
    with pytest.raises(cv.Invalid, match=match):
        generate_main(_write_config(tmp_path, platform, wifi))
