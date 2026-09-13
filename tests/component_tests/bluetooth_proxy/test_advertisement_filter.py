"""The codegen hook external filtering components use to turn on the filter slot."""

from unittest.mock import patch

from esphome.components import bluetooth_proxy


def test_enable_advertisement_filter_emits_define() -> None:
    """External components call this rather than emitting the define."""
    with patch("esphome.codegen.add_define") as add_define:
        bluetooth_proxy.enable_advertisement_filter()

    add_define.assert_called_once_with("USE_BLUETOOTH_PROXY_ADVERTISEMENT_FILTER")
