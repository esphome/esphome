"""Tests for MQTT topics of entities on sub-devices."""

from __future__ import annotations

from collections.abc import Generator
import logging

import pytest

from esphome.components import mqtt
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICES,
    CONF_DISCOVERY,
    CONF_ESPHOME,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MQTT_ID,
    CONF_NAME,
    CONF_STATE_TOPIC,
    CONF_TOPIC_PREFIX,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv
from esphome.helpers import fnv1_hash

DEVICES = {"living_room": "Living Room", "bedroom": "Bedroom"}


@pytest.fixture(autouse=True)
def full_config() -> Generator[Config]:
    original = CORE.unique_ids
    CORE.unique_ids = {}
    config = Config()
    config[CONF_ESPHOME] = {CONF_DEVICES: []}
    token = fv.full_config.set(config)
    for device_id, name in DEVICES.items():
        _add_device(device_id, name)
    yield config
    fv.full_config.reset(token)
    CORE.unique_ids = original


def _add_device(device_id: str, name: str) -> None:
    devices = fv.full_config.get()[CONF_ESPHOME][CONF_DEVICES]
    devices.append({CONF_ID: ID(device_id), CONF_NAME: name})


def _add(
    device_id: str, platform: str, name: str, mqtt: bool = True, **mqtt_options
) -> None:
    config = fv.full_config.get()
    entities = config.setdefault(platform, [])
    entity_id = ID(f"{platform}_{len(CORE.unique_ids)}")
    entity = {CONF_ID: entity_id, CONF_NAME: name, **mqtt_options}
    if (
        mqtt
    ):  # set by cv.OnlyWith(CONF_MQTT_ID, "mqtt") on platforms with an MQTT component
        entity[CONF_MQTT_ID] = ID(f"{entity_id.id}_mqtt")
    entities.append(entity)
    config.declare_ids.append((entity_id, [platform, len(entities) - 1, CONF_ID]))
    object_id = name.lower().replace(" ", "_")
    CORE.unique_ids[(device_id, platform, fnv1_hash(object_id))] = {
        "name": name,
        "device_id": device_id,
        "platform": platform,
        "entity_id": entity_id.id,
        "component": platform,
    }


def _config(
    sub_device_topics: bool = False, topic_prefix: str = "node", discovery: bool = True
) -> dict:
    return {
        mqtt.CONF_SUB_DEVICE_TOPICS: sub_device_topics,
        CONF_TOPIC_PREFIX: topic_prefix,
        CONF_DISCOVERY: discovery,
    }


def test_equal_names_on_different_sub_devices_are_found() -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Temperature")
    _add("", "sensor", "Temperature")
    _add("living_room", "sensor", "Humidity")
    _add(
        "living_room", "binary_sensor", "Temperature"
    )  # another platform: another topic
    assert mqtt._shared_mqtt_ids(_config()) == [
        (
            "default topic and discovery id",
            [
                "sensor 'Temperature' on the main device",
                "sensor 'Temperature' on device 'bedroom'",
                "sensor 'Temperature' on device 'living_room'",
            ],
        )
    ]


def test_warns_when_topics_are_shared(caplog: pytest.LogCaptureFixture) -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Temperature")
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(_config())
    assert "share one default topic and discovery id" in caplog.text
    assert "sub_device_topics: true" in caplog.text


def test_no_warning_without_shared_names(caplog: pytest.LogCaptureFixture) -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Humidity")
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(_config())
    assert caplog.text == ""


def test_sub_device_topics_keep_equal_names_apart(
    caplog: pytest.LogCaptureFixture,
) -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Temperature")
    _add("", "sensor", "Temperature")
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(_config(sub_device_topics=True))
    assert caplog.text == ""


def test_sub_device_and_entity_name_meeting_a_main_device_name_is_an_error() -> None:
    # bedroom_temperature in both discovery ids; the default topics still differ.
    _add("bedroom", "sensor", "Temperature")
    _add("", "sensor", "Bedroom Temperature")
    with pytest.raises(cv.Invalid, match="would share one discovery id,"):
        mqtt._final_validate(_config(sub_device_topics=True))


def test_sub_device_names_that_sanitize_alike_are_an_error() -> None:
    _add_device("battery_1", "Battery A")
    _add_device("battery_2", "Battery_A")
    _add("battery_1", "sensor", "Voltage")
    _add("battery_2", "sensor", "Voltage")
    with pytest.raises(
        cv.Invalid, match="would share one default topic and discovery id"
    ):
        mqtt._final_validate(_config(sub_device_topics=True))


def test_custom_topics_without_a_topic_prefix_still_share_a_discovery_id(
    caplog: pytest.LogCaptureFixture,
) -> None:
    _add("living_room", "sensor", "Temperature", **{CONF_STATE_TOPIC: "a/temp"})
    _add("bedroom", "sensor", "Temperature", **{CONF_STATE_TOPIC: "b/temp"})
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(_config(topic_prefix=""))
    assert "share one discovery id," in caplog.text


@pytest.mark.parametrize(
    ("options", "config"),
    [
        ({}, _config(topic_prefix="")),  # no default topics: not on MQTT at all
        ({CONF_STATE_TOPIC: ""}, _config()),  # an empty topic keeps it off MQTT
        ({CONF_STATE_TOPIC: "own/temp"}, _config(discovery=False)),
        ({CONF_STATE_TOPIC: "own/temp", CONF_DISCOVERY: False}, _config()),
    ],
    ids=[
        "no_topic_prefix",
        "empty_state_topic",
        "discovery_off",
        "entity_discovery_off",
    ],
)
def test_nothing_shared_when_not_published_or_discovered(
    options: dict, config: dict, caplog: pytest.LogCaptureFixture
) -> None:
    _add("living_room", "sensor", "Temperature", **options)
    _add("bedroom", "sensor", "Temperature", **options)
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(config)
    assert caplog.text == ""


def test_default_topics_are_shared_with_discovery_off(
    caplog: pytest.LogCaptureFixture,
) -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Temperature")
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(_config(discovery=False))
    assert "share one default topic," in caplog.text


def test_platforms_without_an_mqtt_component_are_left_out(
    caplog: pytest.LogCaptureFixture,
) -> None:
    _add("living_room", "media_player", "Speaker", mqtt=False)
    _add("bedroom", "media_player", "Speaker", mqtt=False)
    _add("", "media_player", "Bedroom Speaker", mqtt=False)
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(_config())
        mqtt._final_validate(_config(sub_device_topics=True))
    assert caplog.text == ""


def test_internal_entities_are_left_out() -> None:
    _add("bedroom", "sensor", "Temperature")
    _add("", "sensor", "Bedroom Temperature", **{CONF_INTERNAL: True})
    mqtt._final_validate(_config(sub_device_topics=True))


def test_an_entity_whose_config_is_not_found_is_left_out() -> None:
    _add("bedroom", "sensor", "Temperature")
    _add("", "sensor", "Bedroom Temperature")
    fv.full_config.get().declare_ids.pop()  # its id no longer leads to a config
    mqtt._final_validate(_config(sub_device_topics=True))
