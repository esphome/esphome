"""Tests for the openthread component schema."""

import pytest

from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32C6
from esphome.components.openthread import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.core import HexInt
from esphome.yaml_util import Sensitive
from tests.component_tests.types import SetCoreConfigCallable

NETWORK_KEY = 0xDFD34F0F05CAD978EC4E32B0413038FF
PSKC = 0xC23A76E98F1A6483639B1AC1271E2E27
TLV = "0e080000000000010000000300001435060004001fffe0"


@pytest.mark.parametrize(
    ("config", "field", "expected"),
    [
        ({"network_key": hex(NETWORK_KEY)}, "network_key", NETWORK_KEY),
        ({"network_key": hex(NETWORK_KEY), "pskc": hex(PSKC)}, "pskc", PSKC),
        ({"tlv": TLV}, "tlv", TLV),
    ],
)
def test_credentials_are_sensitive(
    config: dict[str, str],
    field: str,
    expected: int | str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Thread credentials are tagged for redaction without changing their value
    or the hexadecimal integer type used to build sdkconfig options."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32C6}
    )
    validated = CONFIG_SCHEMA(config)
    assert isinstance(validated[field], Sensitive)
    assert validated[field] == expected
    assert isinstance(validated[field], HexInt) == isinstance(expected, int)
