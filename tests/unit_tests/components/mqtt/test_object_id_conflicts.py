"""Tests for the MQTT object_id conflict filter.

MQTT still builds default topics and discovery topics from the sanitized
object_id, so entity names that only differ in characters lost during
sanitizing conflict there; _topics_conflict() exempts entities that never
use an object_id-derived topic. See https://github.com/esphome/backlog/issues/85
"""

from pathlib import Path

import pytest

from esphome.components.mqtt import (
    _COMMAND_TOPIC_PLATFORMS,
    _SUB_TOPIC_PLATFORMS,
    _topics_conflict,
)
from esphome.config_validation import Invalid
from esphome.const import (
    CONF_COMMAND_TOPIC,
    CONF_DISCOVERY,
    CONF_NAME,
    CONF_STATE_TOPIC,
    CONF_TOPIC_PREFIX,
)
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    validate_no_object_id_conflicts,
)

COMPONENTS_DIR = Path(__file__).parents[4] / "esphome" / "components"

REASON = "mqtt builds default topics from the entity object_id"


# MQTT infrastructure sources, not entity components
_NON_ENTITY_MQTT_SOURCES = {"mqtt_backend_esp32", "mqtt_client", "mqtt_component"}
# The date, time and datetime MQTT components all belong to the datetime platform
_DATETIME_STEMS = {"date", "time", "datetime"}


def test_command_topic_platforms_in_sync() -> None:
    """Verify _COMMAND_TOPIC_PLATFORMS matches the MQTT components that subscribe.

    Drift silently reintroduces shared subscribe topics, so this derives the set
    from the C++ components that actually call subscribe(); that also catches
    platforms like text that subscribe a command topic without exposing a
    command_topic key in their schema.
    """
    expected: set[str] = set()
    for path in (COMPONENTS_DIR / "mqtt").glob("mqtt_*.cpp"):
        if path.stem in _NON_ENTITY_MQTT_SOURCES:
            continue
        if "this->subscribe" not in path.read_text(encoding="utf-8"):
            continue
        stem = path.stem.removeprefix("mqtt_")
        expected.add("datetime" if stem in _DATETIME_STEMS else stem)
    assert expected == _COMMAND_TOPIC_PLATFORMS


def test_sub_topic_platforms_in_sync() -> None:
    """Verify _SUB_TOPIC_PLATFORMS matches the MQTT components with sub-topics.

    Platforms whose MQTT headers use MQTT_COMPONENT_CUSTOM_TOPIC derive extra
    topics such as position/command from the object_id.
    """
    expected = {
        path.stem.removeprefix("mqtt_")
        for path in (COMPONENTS_DIR / "mqtt").glob("mqtt_*.h")
        if path.stem != "mqtt_component"
        and "MQTT_COMPONENT_CUSTOM_TOPIC" in path.read_text(encoding="utf-8")
    }
    assert expected == _SUB_TOPIC_PLATFORMS


def _register(platform: str, overrides_a: dict, overrides_b: dict) -> None:
    """Register two entities whose names sanitize to the same object_id."""
    validator = entity_duplicate_validator(platform)
    validator({CONF_NAME: "Датчик открытия", **overrides_a})
    validator({CONF_NAME: "Датчик закрытия", **overrides_b})


_MQTT = {CONF_DISCOVERY: True, CONF_TOPIC_PREFIX: "test-device"}
_NO_DISCOVERY = {CONF_DISCOVERY: False}


@pytest.mark.parametrize(
    ("platform", "overrides_a", "overrides_b", "mqtt_config", "conflict"),
    [
        pytest.param(
            "sensor",
            {CONF_STATE_TOPIC: "custom/topic/a", **_NO_DISCOVERY},
            {CONF_STATE_TOPIC: "custom/topic/b", **_NO_DISCOVERY},
            _MQTT,
            False,
            id="custom_state_topics_exempt",
        ),
        pytest.param(
            "switch",
            {CONF_STATE_TOPIC: "custom/topic/a", **_NO_DISCOVERY},
            {CONF_STATE_TOPIC: "custom/topic/b", **_NO_DISCOVERY},
            _MQTT,
            True,
            id="shared_default_command_topic",
        ),
        pytest.param(
            "switch",
            {
                CONF_STATE_TOPIC: "custom/topic/a",
                CONF_COMMAND_TOPIC: "custom/cmd/a",
                **_NO_DISCOVERY,
            },
            {
                CONF_STATE_TOPIC: "custom/topic/b",
                CONF_COMMAND_TOPIC: "custom/cmd/b",
                **_NO_DISCOVERY,
            },
            _MQTT,
            False,
            id="custom_command_topics_exempt",
        ),
        pytest.param(
            "cover",
            {
                CONF_STATE_TOPIC: "custom/topic/a",
                CONF_COMMAND_TOPIC: "custom/cmd/a",
                **_NO_DISCOVERY,
            },
            {
                CONF_STATE_TOPIC: "custom/topic/b",
                CONF_COMMAND_TOPIC: "custom/cmd/b",
                **_NO_DISCOVERY,
            },
            _MQTT,
            True,
            id="sub_topic_platform_always_conflicts",
        ),
        pytest.param(
            "switch",
            {CONF_STATE_TOPIC: "custom/topic/a", **_NO_DISCOVERY},
            {CONF_COMMAND_TOPIC: "custom/cmd/b", **_NO_DISCOVERY},
            _MQTT,
            False,
            id="disjoint_default_topics",
        ),
        pytest.param(
            "sensor",
            {},
            {},
            {CONF_DISCOVERY: False, CONF_TOPIC_PREFIX: ""},
            False,
            id="empty_prefix_no_discovery",
        ),
        pytest.param(
            "sensor",
            {},
            {},
            {CONF_DISCOVERY: True, CONF_TOPIC_PREFIX: ""},
            True,
            id="empty_prefix_discovery_topic",
        ),
    ],
)
def test_topics_conflict(
    platform: str,
    overrides_a: dict,
    overrides_b: dict,
    mqtt_config: dict,
    conflict: bool,
) -> None:
    """Test which colliding object_ids actually share an MQTT topic.

    Custom state and command topics avoid the default topics, disabling discovery
    avoids the discovery topic, an empty topic_prefix disables default topics, and
    platforms with extra object_id sub-topics (cover position/command, ...) always
    conflict.
    """
    _register(platform, overrides_a, overrides_b)
    component_validator = validate_no_object_id_conflicts(
        REASON, conflict_filter=_topics_conflict
    )
    if conflict:
        with pytest.raises(Invalid, match=r"mqtt builds default topics"):
            component_validator(mqtt_config)
    else:
        assert component_validator(mqtt_config) is mqtt_config
