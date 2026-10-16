"""Tests for the button schema presets."""

from collections.abc import Callable

import pytest
import voluptuous as vol

from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_DATABASE,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)

MyButton = button.button_ns.class_("MyButton", button.Button)


def _defaults(schema: cv.Schema) -> dict[str, str]:
    return {
        str(key): key.default()
        for key in schema.schema
        if str(key) in (CONF_DEVICE_CLASS, CONF_ENTITY_CATEGORY, CONF_ICON)
        and key.default is not vol.UNDEFINED
    }


@pytest.mark.parametrize(
    ("preset", "expected"),
    [
        (
            button.factory_reset_button_schema,
            {
                CONF_DEVICE_CLASS: DEVICE_CLASS_RESTART,
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
                CONF_ICON: ICON_RESTART_ALERT,
            },
        ),
        (
            button.restart_button_schema,
            {
                CONF_DEVICE_CLASS: DEVICE_CLASS_RESTART,
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
                CONF_ICON: ICON_RESTART,
            },
        ),
        (
            button.query_button_schema,
            {
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
                CONF_ICON: ICON_DATABASE,
            },
        ),
    ],
)
def test_preset_defaults(
    preset: Callable[..., cv.Schema], expected: dict[str, str]
) -> None:
    assert _defaults(preset(MyButton)) == expected


def test_preset_matches_button_schema() -> None:
    preset = button.restart_button_schema(MyButton)
    manual = button.button_schema(
        MyButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_RESTART,
    )
    assert [str(k) for k in preset.schema] == [str(k) for k in manual.schema]
    assert _defaults(preset) == _defaults(manual)
