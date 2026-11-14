"""Tests for the Zigbee component validator behavior."""

from __future__ import annotations

from unittest.mock import patch

import pytest

from esphome.components import zigbee
from esphome.components.zigbee.const import CONF_ZIGBEE_ID, ZigbeeComponent
from esphome.const import CONF_INTERNAL, KEY_CORE, KEY_TARGET_PLATFORM
from esphome.core import CORE, ID


@pytest.mark.parametrize(
    ("validator_name", "entity_key", "entity_type"),
    [
        ("validate_binary_sensor", "zigbee_binary_sensor", "ZigbeeBinarySensor"),
        ("validate_sensor", "zigbee_sensor", "ZigbeeSensor"),
        ("validate_switch", "zigbee_switch", "ZigbeeSwitch"),
        ("validate_number", "zigbee_number", "ZigbeeNumber"),
    ],
)
@pytest.mark.parametrize(
    ("platform", "internal", "inject_ids"),
    [("nrf52", False, True), ("esp32", False, False), ("nrf52", True, False)],
)
def test_zigbee_entity_validators_inject_ids(
    validator_name: str,
    entity_key: str,
    entity_type: str,
    platform: str,
    internal: bool,
    inject_ids: bool,
) -> None:
    """Zigbee entity validators add Zephyr IDs only for public nrf52 entities."""
    config = {"cluster": "generic"}
    if internal:
        config[CONF_INTERNAL] = True

    with (
        patch.object(CORE, "data", {KEY_CORE: {KEY_TARGET_PLATFORM: platform}}),
        patch.object(CORE, "loaded_integrations", {"zigbee"}),
    ):
        validated = getattr(zigbee, validator_name)(config)

    assert validated is config
    if inject_ids:
        assert isinstance(config[CONF_ZIGBEE_ID], ID)
        assert config[CONF_ZIGBEE_ID].type == ZigbeeComponent

        entity_id = config[entity_key]
        assert isinstance(entity_id, ID)
        assert entity_id.type == getattr(zigbee, entity_type)
    else:
        assert CONF_ZIGBEE_ID not in config
        assert entity_key not in config
