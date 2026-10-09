"""The codegen hook external filtering components use to turn on the filter slot."""

from esphome.components import bluetooth_proxy
from esphome.core import CORE


def test_enable_advertisement_filter_emits_define() -> None:
    """External components call this rather than emitting the define."""
    bluetooth_proxy.enable_advertisement_filter()

    assert "USE_BLUETOOTH_PROXY_ADVERTISEMENT_FILTER" in {
        define.name for define in CORE.defines
    }
