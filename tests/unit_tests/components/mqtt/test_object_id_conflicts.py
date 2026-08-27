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
from esphome.core import CORE
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    validate_no_object_id_conflicts,
)

COMPONENTS_DIR = Path(__file__).parents[4] / "esphome" / "components"

REASON = "mqtt builds default topics from the entity object_id"


# MQTT infrastructure sources, not entity components
_NON_ENTITY_MQTT_SOURCES = {"mqtt_client", "mqtt_component"}
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


def test_conflict_filter_exempts_custom_topics() -> None:
    """Test that custom state topics with discovery off avoid the conflict."""
    validator = entity_duplicate_validator("sensor")
    # Both entities have custom state topics and discovery disabled per entity,
    # so no object_id-derived MQTT topic is used
    validator(
        {
            CONF_NAME: "Датчик открытия",
            CONF_STATE_TOPIC: "custom/topic/a",
            CONF_DISCOVERY: False,
        }
    )
    validator(
        {
            CONF_NAME: "Датчик закрытия",
            CONF_STATE_TOPIC: "custom/topic/b",
            CONF_DISCOVERY: False,
        }
    )

    component_validator = validate_no_object_id_conflicts(
        REASON, conflict_filter=_topics_conflict
    )
    config: dict = {CONF_DISCOVERY: True, CONF_TOPIC_PREFIX: "test-device"}
    assert component_validator(config) is config

    # Without the filter the same conflicts are fatal
    with pytest.raises(Invalid, match=r"mqtt builds default topics"):
        validate_no_object_id_conflicts(REASON)({})


def test_conflict_on_default_command_topic() -> None:
    """Test that commandable platforms conflict through their default command topic.

    Custom state topics with discovery off are not enough for platforms that also
    subscribe to an object_id-derived command topic.
    """
    validator = entity_duplicate_validator("switch")
    validator(
        {
            CONF_NAME: "Датчик открытия",
            CONF_STATE_TOPIC: "custom/topic/a",
            CONF_DISCOVERY: False,
        }
    )
    validator(
        {
            CONF_NAME: "Датчик закрытия",
            CONF_STATE_TOPIC: "custom/topic/b",
            CONF_DISCOVERY: False,
        }
    )

    component_validator = validate_no_object_id_conflicts(
        REASON, conflict_filter=_topics_conflict
    )
    mqtt_config: dict = {CONF_DISCOVERY: True, CONF_TOPIC_PREFIX: "test-device"}
    # Both switches share the default command topic: rejected
    with pytest.raises(Invalid, match=r"mqtt builds default topics"):
        component_validator(mqtt_config)

    # With custom command topics as well, nothing derives from the object_id
    CORE.reset()
    validator = entity_duplicate_validator("switch")
    validator(
        {
            CONF_NAME: "Датчик открытия",
            CONF_STATE_TOPIC: "custom/topic/a",
            CONF_COMMAND_TOPIC: "custom/cmd/a",
            CONF_DISCOVERY: False,
        }
    )
    validator(
        {
            CONF_NAME: "Датчик закрытия",
            CONF_STATE_TOPIC: "custom/topic/b",
            CONF_COMMAND_TOPIC: "custom/cmd/b",
            CONF_DISCOVERY: False,
        }
    )
    assert component_validator(mqtt_config) is mqtt_config


def test_conflict_on_sub_topic_platforms() -> None:
    """Test that platforms with extra object_id sub-topics always conflict.

    Covers derive topics like position/command from the object_id through their
    own config keys, so custom state and command topics cannot exempt them.
    """
    validator = entity_duplicate_validator("cover")
    validator(
        {
            CONF_NAME: "Датчик открытия",
            CONF_STATE_TOPIC: "custom/topic/a",
            CONF_COMMAND_TOPIC: "custom/cmd/a",
            CONF_DISCOVERY: False,
        }
    )
    validator(
        {
            CONF_NAME: "Датчик закрытия",
            CONF_STATE_TOPIC: "custom/topic/b",
            CONF_COMMAND_TOPIC: "custom/cmd/b",
            CONF_DISCOVERY: False,
        }
    )

    component_validator = validate_no_object_id_conflicts(
        REASON, conflict_filter=_topics_conflict
    )
    with pytest.raises(Invalid, match=r"mqtt builds default topics"):
        component_validator({CONF_DISCOVERY: True, CONF_TOPIC_PREFIX: "test-device"})


def test_no_conflict_on_disjoint_default_topics() -> None:
    """Test that entities whose default topics are disjoint do not conflict.

    One entity uses only the default command topic and the other only the default
    state topic, so they never share a topic.
    """
    validator = entity_duplicate_validator("switch")
    validator(
        {
            CONF_NAME: "Датчик открытия",
            CONF_STATE_TOPIC: "custom/topic/a",
            CONF_DISCOVERY: False,
        }
    )
    validator(
        {
            CONF_NAME: "Датчик закрытия",
            CONF_COMMAND_TOPIC: "custom/cmd/b",
            CONF_DISCOVERY: False,
        }
    )

    component_validator = validate_no_object_id_conflicts(
        REASON, conflict_filter=_topics_conflict
    )
    config: dict = {CONF_DISCOVERY: True, CONF_TOPIC_PREFIX: "test-device"}
    assert component_validator(config) is config


def test_no_conflict_on_empty_topic_prefix() -> None:
    """Test that an empty topic_prefix disables the default topic conflict.

    With topic_prefix set to null no default topics exist at runtime, so entities
    without custom state topics cannot conflict; only discovery still matters.
    """
    validator = entity_duplicate_validator("sensor")
    validator({CONF_NAME: "Датчик открытия"})
    validator({CONF_NAME: "Датчик закрытия"})

    component_validator = validate_no_object_id_conflicts(
        REASON, conflict_filter=_topics_conflict
    )
    # No default topics and no discovery: valid
    config: dict = {CONF_DISCOVERY: False, CONF_TOPIC_PREFIX: ""}
    assert component_validator(config) is config

    # Discovery still uses object_id-derived config topics: rejected
    with pytest.raises(Invalid, match=r"mqtt builds default topics"):
        component_validator({CONF_DISCOVERY: True, CONF_TOPIC_PREFIX: ""})
