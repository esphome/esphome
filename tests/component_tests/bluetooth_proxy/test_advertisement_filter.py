"""The codegen hook external filtering components use to turn on the filter slot."""

from unittest.mock import patch

from esphome.components import bluetooth_proxy


def test_enable_advertisement_filter_emits_define() -> None:
    """The function is the supported API; the define behind it may be renamed.

    External components must call this rather than emitting the define, so that
    the define stays an implementation detail core is free to change.
    """
    with patch("esphome.codegen.add_define") as add_define:
        bluetooth_proxy.enable_advertisement_filter()

    add_define.assert_called_once_with("USE_BLUETOOTH_PROXY_ADVERTISEMENT_FILTER")
