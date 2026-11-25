"""A template switch can set its wizard role, written in full as template.<name>."""

from collections.abc import Callable
import json
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.api import wizard
from esphome.components.template import CONF_WIZARD_ROLE
from esphome.components.template.switch import CONFIG_SCHEMA
from esphome.const import CONF_ID, CONF_NAME, CONF_OPTIMISTIC, PlatformFramework
from esphome.helpers import zstd_module
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

LONGEST_ROLE = "template." + "a" * (wizard.WIZARD_ROLE_MAX_LENGTH - len("template."))


def _switch_config(role: str) -> ConfigType:
    return {
        CONF_ID: "bluetooth_switch",
        CONF_NAME: "Bluetooth",
        CONF_OPTIMISTIC: True,
        CONF_WIZARD_ROLE: role,
    }


def test_role_is_sent_in_the_wizard(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("switch_wizard_role.yaml"))

    match = re.search(
        r"const uint8_t esphome::api::API_WIZARD_DATA\[\] PROGMEM = \{([^}]*)\};",
        main_cpp,
    )
    assert match is not None
    blob = bytes(int(byte) for byte in match.group(1).split(", "))
    document = json.loads(zstd_module().decompress(blob))
    assert [entity.get("role") for entity in document["pages"][0]["entities"]] == [
        "template.bluetooth_enabled",
        None,
    ]


@pytest.mark.parametrize(
    "role",
    [
        "",
        "template",
        "template.",
        "template.Bluetooth",
        "template.bluetooth enabled",
        "template.bluetooth.enabled",
        LONGEST_ROLE + "a",
        "bluetooth_enabled",
        "bluetooth.enabled",
        "sendspin.enabled",
        "templates.enabled",
    ],
)
def test_other_roles_are_rejected(
    set_core_config: SetCoreConfigCallable, role: str
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(
        cv.Invalid, match="A wizard role of a template entity is template.<name>"
    ):
        CONFIG_SCHEMA(_switch_config(role))


def test_longest_role_is_accepted(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    CONFIG_SCHEMA(_switch_config(LONGEST_ROLE))
