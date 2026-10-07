"""Tests for MQTT topics of entities on sub-devices."""

from __future__ import annotations

from collections.abc import Generator
import logging

import pytest

from esphome.components import mqtt
from esphome.const import CONF_TOPIC_PREFIX
from esphome.core import CORE
from esphome.helpers import fnv1_hash


@pytest.fixture(autouse=True)
def entities() -> Generator[dict]:
    original = CORE.unique_ids
    CORE.unique_ids = {}
    yield CORE.unique_ids
    CORE.unique_ids = original


def _add(device_id: str, platform: str, name: str) -> None:
    object_id = name.lower().replace(" ", "_")
    CORE.unique_ids[(device_id, platform, fnv1_hash(object_id))] = {
        "name": name,
        "device_id": device_id,
        "platform": platform,
        "entity_id": "unknown",
        "component": platform,
    }


def _config(sub_device_topics: bool = False, topic_prefix: str = "node") -> dict:
    return {
        mqtt.CONF_SUB_DEVICE_TOPICS: sub_device_topics,
        CONF_TOPIC_PREFIX: topic_prefix,
    }


def test_equal_names_on_different_sub_devices_are_found() -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Temperature")
    _add("", "sensor", "Temperature")
    _add("living_room", "sensor", "Humidity")
    _add(
        "living_room", "binary_sensor", "Temperature"
    )  # another platform: another topic
    assert mqtt._shared_sub_device_topics() == [
        [
            "sensor 'Temperature' on the main device",
            "sensor 'Temperature' on device 'bedroom'",
            "sensor 'Temperature' on device 'living_room'",
        ]
    ]


def test_warns_when_topics_are_shared(caplog: pytest.LogCaptureFixture) -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Temperature")
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(_config())
    assert "share one default topic" in caplog.text
    assert "sub_device_topics: true" in caplog.text


@pytest.mark.parametrize(
    "config",
    [_config(sub_device_topics=True), _config(topic_prefix="")],
    ids=["sub_device_topics", "no_default_topics"],
)
def test_no_warning_when_topics_are_not_shared(
    config: dict, caplog: pytest.LogCaptureFixture
) -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Temperature")
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(config)
    assert caplog.text == ""


def test_no_warning_without_shared_names(caplog: pytest.LogCaptureFixture) -> None:
    _add("living_room", "sensor", "Temperature")
    _add("bedroom", "sensor", "Humidity")
    with caplog.at_level(logging.WARNING):
        mqtt._final_validate(_config())
    assert caplog.text == ""
