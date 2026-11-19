"""MQTT topic rules shared by code generation and the MQTT log viewer."""

from esphome.const import (
    CONF_BIRTH_MESSAGE,
    CONF_LOG_TOPIC,
    CONF_SHUTDOWN_MESSAGE,
    CONF_TOPIC,
    CONF_TOPIC_PREFIX,
    CONF_WILL_MESSAGE,
)
from esphome.types import ConfigType

# Topic suffix of each message whose topic defaults to one under the topic prefix
DEFAULT_TOPIC_SUFFIXES = {
    CONF_BIRTH_MESSAGE: "/status",
    CONF_WILL_MESSAGE: "/status",
    CONF_SHUTDOWN_MESSAGE: "/status",
    CONF_LOG_TOPIC: "/debug",
}


def prefix_gets_mac_suffix(topic_prefix: str, name: str, mac_suffix: bool) -> bool:
    """Whether the firmware appends the MAC suffix to this topic prefix at runtime."""
    return mac_suffix and topic_prefix == name


def per_board_suffix(
    mqtt_config: ConfigType, key: str, name: str, mac_suffix: bool
) -> str | None:
    """Topic suffix of a message the firmware sends on App.get_name() + suffix, else None.

    A message missing from the config counts as the one defaulted from the topic prefix.
    """
    topic_prefix = mqtt_config.get(CONF_TOPIC_PREFIX, name)
    if not prefix_gets_mac_suffix(topic_prefix, name, mac_suffix):
        return None
    suffix = DEFAULT_TOPIC_SUFFIXES[key]
    if (
        key in mqtt_config
        and (mqtt_config[key] or {}).get(CONF_TOPIC) != f"{topic_prefix}{suffix}"
    ):
        return None
    return suffix
