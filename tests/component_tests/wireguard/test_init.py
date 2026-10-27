"""Tests for the wireguard component schema."""

import pytest

from esphome.components.wireguard import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.yaml_util import SensitiveStr
from tests.component_tests.types import SetCoreConfigCallable

# Any 42 base64 chars plus a valid terminator satisfies _WG_KEY_REGEX.
PRIVATE_KEY = "a" * 42 + "A="
PEER_PUBLIC_KEY = "b" * 42 + "A="
PEER_PRESHARED_KEY = "c" * 42 + "A="


@pytest.mark.parametrize(
    ("field", "value", "sensitive"),
    [
        ("private_key", PRIVATE_KEY, True),
        ("peer_preshared_key", PEER_PRESHARED_KEY, True),
        ("peer_public_key", PEER_PUBLIC_KEY, False),
    ],
)
def test_key_sensitivity(
    field: str,
    value: str,
    sensitive: bool,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The private and preshared keys are secrets and must be tagged so dump
    tooling redacts them deterministically; the peer's public key is not a
    secret and must stay readable in redacted dumps (see issue #17718)."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA(
        {
            "address": "10.0.0.2",
            "private_key": PRIVATE_KEY,
            "peer_endpoint": "wg.example.com",
            "peer_public_key": PEER_PUBLIC_KEY,
            "peer_preshared_key": PEER_PRESHARED_KEY,
        }
    )
    assert isinstance(config[field], SensitiveStr) == sensitive
    assert config[field] == value
