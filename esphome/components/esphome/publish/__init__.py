from __future__ import annotations

import logging
import os
from pathlib import Path
import subprocess
from typing import TYPE_CHECKING

from esphome.const import (
    CONF_BROKER,
    CONF_ESPHOME,
    CONF_MQTT,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_PUBLISH_SHELL_COMMAND,
    CONF_TOPIC_PREFIX,
    CONF_USERNAME,
)
from esphome.core import CORE, EsphomeError

if TYPE_CHECKING:
    from esphome.types import ConfigType

CODEOWNERS = ["@AndreKR"]

_LOGGER = logging.getLogger(__name__)

_MQTT_DEFAULT_PORT = 1883


def upload_program(config: ConfigType, _args, host: str) -> bool:
    if host != "PUBLISH":
        return False

    publish_shell_command = config[CONF_ESPHOME].get(CONF_PUBLISH_SHELL_COMMAND)
    if not publish_shell_command:
        raise EsphomeError("No publish_shell_command configured under esphome:")

    env = os.environ.copy()
    env["ESPHOME_DEVICE_NAME"] = CORE.name
    env["ESPHOME_FIRMWARE_BIN"] = str(CORE.firmware_bin)
    if CONF_MQTT in config:
        mqtt = config[CONF_MQTT]
        env["ESPHOME_MQTT_BROKER"] = str(mqtt.get(CONF_BROKER, ""))
        env["ESPHOME_MQTT_PORT"] = str(mqtt.get(CONF_PORT, _MQTT_DEFAULT_PORT))
        env["ESPHOME_MQTT_USERNAME"] = str(mqtt.get(CONF_USERNAME, ""))
        env["ESPHOME_MQTT_PASSWORD"] = str(mqtt.get(CONF_PASSWORD, ""))
        env["ESPHOME_MQTT_TOPIC_PREFIX"] = str(mqtt.get(CONF_TOPIC_PREFIX, CORE.name))

    _LOGGER.info("Running publish command: %s", publish_shell_command)
    exit_code = subprocess.call(
        publish_shell_command,
        shell=True,
        env=env,
        cwd=Path(CORE.config_path).parent,
    )
    if exit_code != 0:
        raise EsphomeError(f"Publish command exited with code {exit_code}")
    return True
