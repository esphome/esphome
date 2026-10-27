"""Tests for the usb_host FIFO split sized from periodic packet requests."""

import pytest

from esphome import config_validation as cv
from esphome.components import usb_host
from esphome.components.esp32 import (
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
)
from esphome.config import Config
from esphome.const import (
    CONF_FRAMEWORK,
    KEY_IDF_VERSION,
    KEY_VARIANT,
    PlatformFramework,
)
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

IDF_5_5 = cv.Version(5, 5, 6)


def _set_variant(
    set_core_config: SetCoreConfigCallable, variant: str, idf: cv.Version = IDF_5_5
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant, KEY_IDF_VERSION: idf},
    )


def test_no_request_keeps_the_default_split(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    assert usb_host.fifo_split() is None


def test_in_and_out_requests_share_the_spare_lines(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # 196-byte packets each way, such as 48 kHz 16-bit stereo audio: 49 + 51 lines, plus 84 spare.
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(in_bytes=196, out_bytes=196)
    rx, nptx, ptx = usb_host.fifo_split()
    assert (rx, nptx, ptx) == (93, 16, 91)
    assert rx + nptx + ptx == 200
    assert ptx * 4 >= 196
    assert (rx - 2) * 4 >= 196


def test_out_request_gets_the_rest(set_core_config: SetCoreConfigCallable) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S2)
    usb_host.request_periodic_fifo(out_bytes=196)
    assert usb_host.fifo_split() == (34, 16, 150)


def test_in_request_gets_the_rest(set_core_config: SetCoreConfigCallable) -> None:
    # 512 bytes is more than the default 408 bytes of RX.
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(in_bytes=512)
    assert usb_host.fifo_split() == (152, 16, 32)


@pytest.mark.parametrize(
    ("in_bytes", "out_bytes"), [(0, 0), (196, 0), (408, 128), (0, 128)]
)
def test_requests_that_fit_the_default_keep_it(
    set_core_config: SetCoreConfigCallable, in_bytes: int, out_bytes: int
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(in_bytes=in_bytes, out_bytes=out_bytes)
    assert usb_host.fifo_split() is None


def test_requests_that_fit_the_default_need_no_newer_idf(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3, idf=cv.Version(5, 4, 2))
    usb_host.request_periodic_fifo(in_bytes=34, out_bytes=128)
    usb_host.FINAL_VALIDATE_SCHEMA({})
    assert "USE_USB_HOST_FIFO_CONFIG" not in _defines()


def test_requests_keep_the_largest_packet(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(out_bytes=400)
    usb_host.request_periodic_fifo(in_bytes=96, out_bytes=196)
    assert CORE.data[usb_host.DOMAIN][usb_host.KEY_PERIODIC_FIFO] == [96, 400]


@pytest.mark.parametrize(
    ("variant", "bias", "split"),
    [
        # The splits ESP-IDF's _calculate_fifo_from_bias() picks
        (VARIANT_ESP32S3, None, (104, 64, 32)),
        (VARIANT_ESP32S3, "CONFIG_USB_HOST_HW_BUFFER_BIAS_IN", (152, 16, 32)),
        (VARIANT_ESP32S3, "CONFIG_USB_HOST_HW_BUFFER_BIAS_PERIODIC_OUT", (34, 16, 150)),
        (VARIANT_ESP32P4, None, (512, 256, 128)),
        (VARIANT_ESP32P4, "CONFIG_USB_HOST_HW_BUFFER_BIAS_IN", (704, 64, 128)),
        (
            VARIANT_ESP32P4,
            "CONFIG_USB_HOST_HW_BUFFER_BIAS_PERIODIC_OUT",
            (130, 64, 702),
        ),
    ],
)
def test_bias_splits_match_esp_idf(
    variant: str, bias: str | None, split: tuple[int, int, int]
) -> None:
    assert usb_host.FIFO_GEOMETRY[variant].bias_split(bias) == split


def test_high_speed_request_that_fits_the_default_keeps_it(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The P4's default split has 512 bytes of periodic TX.
    _set_variant(set_core_config, VARIANT_ESP32P4)
    usb_host.request_periodic_fifo(in_bytes=1024, out_bytes=512)
    assert usb_host.fifo_split() is None


def test_high_speed_requests_share_the_spare_lines(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # 1024-byte interrupt packets each way, as a Stream Deck XL uses: 258 + 256 lines, plus 318 spare.
    _set_variant(set_core_config, VARIANT_ESP32P4)
    usb_host.request_periodic_fifo(in_bytes=1024, out_bytes=1024)
    rx, nptx, ptx = usb_host.fifo_split()
    assert (rx, nptx, ptx) == (417, 64, 415)
    assert rx + nptx + ptx == 896


def test_high_speed_requests_that_do_not_fit_are_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32P4)
    usb_host.request_periodic_fifo(in_bytes=2048, out_bytes=2048)
    with pytest.raises(cv.Invalid, match="the ESP32P4 has 3584 usable"):
        usb_host.fifo_split()


@pytest.mark.parametrize("variant", [VARIANT_ESP32H4, VARIANT_ESP32S31])
def test_variants_without_known_geometry_are_not_sized(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    _set_variant(set_core_config, variant)
    usb_host.request_periodic_fifo(out_bytes=1024)
    assert usb_host.fifo_split() is None


def test_requests_that_do_not_fit_are_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(in_bytes=776, out_bytes=776)
    with pytest.raises(cv.Invalid, match="bytes of USB FIFO"):
        usb_host.fifo_split()


def _defines() -> dict[str, str]:
    return {define.name: str(define.value) for define in CORE.defines}


def test_final_validate_emits_the_split(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(out_bytes=196)
    usb_host.FINAL_VALIDATE_SCHEMA({})
    defines = _defines()
    assert "USE_USB_HOST_FIFO_CONFIG" in defines
    assert (
        defines["USB_HOST_FIFO_RX_LINES"],
        defines["USB_HOST_FIFO_NPTX_LINES"],
        defines["USB_HOST_FIFO_PTX_LINES"],
    ) == ("34", "16", "150")


def _set_bias(set_core_config: SetCoreConfigCallable, option: str) -> None:
    full = Config()
    full["esp32"] = {CONF_FRAMEWORK: {"sdkconfig_options": {option: "y"}}}
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: VARIANT_ESP32S3, KEY_IDF_VERSION: IDF_5_5},
        full_config=full,
    )


def test_request_that_fits_the_selected_bias_keeps_it(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The IN bias leaves RX 152 lines, enough for 512-byte IN packets.
    _set_bias(set_core_config, "CONFIG_USB_HOST_HW_BUFFER_BIAS_IN")
    usb_host.request_periodic_fifo(in_bytes=512)
    assert usb_host.fifo_split() is None


@pytest.mark.parametrize(
    ("option", "in_bytes", "out_bytes"),
    [
        # The IN bias reserves 152 RX lines, leaving no room for more than 128-byte OUT packets.
        ("CONFIG_USB_HOST_HW_BUFFER_BIAS_IN", 0, 196),
        # The periodic OUT bias reserves 150 PTX lines, leaving no room for more than 128-byte IN packets.
        ("CONFIG_USB_HOST_HW_BUFFER_BIAS_PERIODIC_OUT", 196, 0),
    ],
)
def test_request_conflicting_with_the_selected_bias_is_rejected(
    set_core_config: SetCoreConfigCallable, option: str, in_bytes: int, out_bytes: int
) -> None:
    _set_bias(set_core_config, option)
    usb_host.request_periodic_fifo(in_bytes=in_bytes, out_bytes=out_bytes)
    with pytest.raises(cv.Invalid, match=f"FIFO split {option} selects"):
        usb_host.fifo_split()


@pytest.mark.parametrize("idf", [cv.Version(5, 3, 4), cv.Version(5, 4, 3)])
def test_final_validate_needs_idf_5_5(
    set_core_config: SetCoreConfigCallable, idf: cv.Version
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3, idf=idf)
    usb_host.request_periodic_fifo(out_bytes=196)
    with pytest.raises(cv.Invalid, match="requires ESP-IDF 5.5.0 or newer"):
        usb_host.FINAL_VALIDATE_SCHEMA({})
