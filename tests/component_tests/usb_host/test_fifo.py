"""Tests for the FIFO split usb_host sizes from periodic packet requests."""

import pytest

from esphome import config_validation as cv
from esphome.components import usb_host
from esphome.components.esp32 import VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3
from esphome.const import KEY_IDF_VERSION, KEY_VARIANT, PlatformFramework
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
    # 196-byte packets each way, such as 48 kHz 16-bit stereo audio: 49 + 51 lines, plus 140 spare.
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(in_bytes=196, out_bytes=196)
    rx, nptx, ptx = usb_host.fifo_split()
    assert (rx, nptx, ptx) == (121, 16, 119)
    assert rx + nptx + ptx == 256
    assert ptx * 4 >= 196
    assert (rx - 2) * 4 >= 196


def test_out_request_gets_the_rest(set_core_config: SetCoreConfigCallable) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S2)
    usb_host.request_periodic_fifo(out_bytes=196)
    assert usb_host.fifo_split() == (34, 16, 206)


def test_in_request_gets_the_rest(set_core_config: SetCoreConfigCallable) -> None:
    # 776 bytes, such as 96 kHz 32-bit stereo audio, is more than the default 632 bytes of RX.
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(in_bytes=776)
    assert usb_host.fifo_split() == (208, 16, 32)


@pytest.mark.parametrize(
    ("in_bytes", "out_bytes"), [(0, 0), (196, 0), (632, 128), (0, 128)]
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
    _set_variant(set_core_config, VARIANT_ESP32S3, idf=cv.Version(5, 4, 3))
    usb_host.request_periodic_fifo(in_bytes=34, out_bytes=128)
    usb_host.FINAL_VALIDATE_SCHEMA({})
    assert usb_host.KEY_FIFO_SPLIT not in CORE.data[usb_host.DOMAIN]


def test_requests_keep_the_largest_packet(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(out_bytes=400)
    usb_host.request_periodic_fifo(in_bytes=96, out_bytes=196)
    assert CORE.data[usb_host.DOMAIN][usb_host.KEY_PERIODIC_FIFO] == [96, 400]


def test_only_full_speed_variants_are_sized(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32P4)
    usb_host.request_periodic_fifo(out_bytes=196)
    assert usb_host.fifo_split() is None


def test_requests_that_do_not_fit_are_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(in_bytes=776, out_bytes=776)
    with pytest.raises(cv.Invalid, match="bytes of USB FIFO"):
        usb_host.fifo_split()


def test_final_validate_stores_the_split(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3)
    usb_host.request_periodic_fifo(out_bytes=196)
    usb_host.FINAL_VALIDATE_SCHEMA({})
    assert CORE.data[usb_host.DOMAIN][usb_host.KEY_FIFO_SPLIT] == (34, 16, 206)


def test_final_validate_needs_idf_5_5(set_core_config: SetCoreConfigCallable) -> None:
    _set_variant(set_core_config, VARIANT_ESP32S3, idf=cv.Version(5, 4, 3))
    usb_host.request_periodic_fifo(out_bytes=196)
    with pytest.raises(cv.Invalid, match="needs ESP-IDF 5.5.0 or later"):
        usb_host.FINAL_VALIDATE_SCHEMA({})
