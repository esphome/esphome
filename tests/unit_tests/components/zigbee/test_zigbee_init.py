"""Tests for the Zigbee component validator behavior."""

from __future__ import annotations

from unittest.mock import patch

import pytest

from esphome.components import zigbee
from esphome.components.zigbee.const import CONF_ZIGBEE_ID, ZigbeeComponent
from esphome.const import CONF_INTERNAL, CONF_MODEL, KEY_CORE, KEY_TARGET_PLATFORM
from esphome.core import CORE, ID


def test_model_default_uses_current_node_name() -> None:
    """The model default is resolved when the Zigbee schema is validated."""
    with patch.object(CORE, "name", "test_node"):
        validated = zigbee.CONFIG_SCHEMA.validators[0]({})

    assert validated[CONF_MODEL] == "test_node"


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
    """Zigbee entity validators add IDs for public nrf52 entities and esp32 numbers."""
    # ESP32 numbers are ZigbeeNumber components too
    inject_ids |= platform == "esp32" and entity_key == "zigbee_number"
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
        assert config[CONF_ZIGBEE_ID].type is ZigbeeComponent

        entity_id = config[entity_key]
        assert isinstance(entity_id, ID)
        assert entity_id.type is getattr(zigbee, entity_type)
    else:
        assert CONF_ZIGBEE_ID not in config
        assert entity_key not in config
