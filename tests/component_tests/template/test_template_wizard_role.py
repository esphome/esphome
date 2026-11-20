"""A template switch can name its wizard role, which is sent as template.<name>."""

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

MAX_LENGTH = wizard.WIZARD_ROLE_MAX_LENGTH - len("template.")


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
        "Bluetooth",
        "template.bluetooth",
        "bluetooth enabled",
        "a" * (MAX_LENGTH + 1),
    ],
)
def test_invalid_role_names_are_rejected(
    set_core_config: SetCoreConfigCallable, role: str
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match=f"A wizard role is 1 to {MAX_LENGTH} "):
        CONFIG_SCHEMA(_switch_config(role))


def test_longest_role_name_is_accepted(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    CONFIG_SCHEMA(_switch_config("a" * MAX_LENGTH))
