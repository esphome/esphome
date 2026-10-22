from collections.abc import Callable

import esphome.codegen as cg
from esphome.components import event
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_DOUBLE_CLICK_IRQ,
    CONF_INTERRUPT_PIN,
    CONF_M5PM1_ID,
    CONF_POWER_BUTTON,
    CONF_SINGLE_CLICK_IRQ,
    M5PM1Component,
    m5pm1_ns,
)

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["m5pm1"]

CONF_EVENTS = "events"

M5PM1Event = m5pm1_ns.class_("M5PM1Event", event.Event, cg.Component)
M5PM1EventType = m5pm1_ns.enum("M5PM1EventType", is_class=True)

POWER_EVENTS = {
    "vin_inserted": 1 << 0,
    "vin_removed": 1 << 1,
    "vinout_inserted": 1 << 2,
    "vinout_removed": 1 << 3,
    "battery_inserted": 1 << 4,
    "battery_removed": 1 << 5,
}

USER_BUTTON_EVENTS = {
    "single_click": 1 << 0,
    "double_click": 1 << 2,
}

SYSTEM_IRQ_EVENTS = {"wakeup": 1 << 1}

WAKE_SOURCE_EVENTS = {
    "timer_wakeup": 1 << 0,
    "vin_wakeup": 1 << 1,
    "power_button_wakeup": 1 << 2,
    "reset_button_wakeup": 1 << 3,
    "command_reset_wakeup": 1 << 4,
    "gpio_wakeup": 1 << 5,
    "vinout_wakeup": 1 << 6,
}

SYSTEM_EVENTS = {
    **SYSTEM_IRQ_EVENTS,
    **dict.fromkeys(WAKE_SOURCE_EVENTS, SYSTEM_IRQ_EVENTS["wakeup"]),
}

ALL_EVENTS = tuple(POWER_EVENTS) + tuple(SYSTEM_EVENTS) + tuple(USER_BUTTON_EVENTS)

EVENT_GROUPS = {
    "power": (M5PM1EventType.M5PM1_EVENT_TYPE_POWER, POWER_EVENTS),
    "system": (M5PM1EventType.M5PM1_EVENT_TYPE_SYSTEM, SYSTEM_EVENTS),
    "user_button": (
        M5PM1EventType.M5PM1_EVENT_TYPE_USER_BUTTON,
        USER_BUTTON_EVENTS,
    ),
}


def _validate_unique_events(events: list[str]) -> list[str]:
    if len(events) != len(set(events)):
        raise cv.Invalid("Events must be unique")
    return events


def _validate_event_group(config: ConfigType) -> ConfigType:
    valid_events = EVENT_GROUPS[config[CONF_TYPE]][1]
    if CONF_EVENTS not in config:
        config[CONF_EVENTS] = list(valid_events)
        return config

    invalid_events = [name for name in config[CONF_EVENTS] if name not in valid_events]
    if invalid_events:
        raise cv.Invalid(
            f"Event type '{config[CONF_TYPE]}' does not support: "
            f"{', '.join(invalid_events)}",
            path=[CONF_EVENTS],
        )
    return config


CONFIG_SCHEMA = cv.All(
    event.event_schema(M5PM1Event).extend(
        {
            cv.Required(CONF_M5PM1_ID): cv.use_id(M5PM1Component),
            cv.Required(CONF_TYPE): cv.one_of(*EVENT_GROUPS, lower=True),
            cv.Optional(CONF_EVENTS): cv.All(
                cv.ensure_list(cv.one_of(*ALL_EVENTS, lower=True)),
                cv.Length(min=1),
                _validate_unique_events,
            ),
        }
    ),
    _validate_event_group,
)


def _validate_parent_config(
    event_config: ConfigType,
) -> Callable[[ConfigType], ConfigType]:
    def validator(parent_config: ConfigType) -> ConfigType:
        if CONF_INTERRUPT_PIN not in parent_config:
            raise cv.Invalid("M5PM1 events require interrupt_pin and interrupt_gpio")

        if event_config[CONF_TYPE] != "user_button":
            return parent_config

        power_button = parent_config[CONF_POWER_BUTTON]
        if (
            "single_click" in event_config[CONF_EVENTS]
            and not power_button[CONF_SINGLE_CLICK_IRQ]
        ):
            raise cv.Invalid(
                "single_click events require power_button.single_click_irq: true"
            )
        if (
            "double_click" in event_config[CONF_EVENTS]
            and not power_button[CONF_DOUBLE_CLICK_IRQ]
        ):
            raise cv.Invalid(
                "double_click events require power_button.double_click_irq: true"
            )
        return parent_config

    return validator


def _final_validate(config: ConfigType) -> ConfigType:
    fv.id_declaration_match_schema(_validate_parent_config(config))(
        config[CONF_M5PM1_ID]
    )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_M5PM1_ID])
    event_type, event_map = EVENT_GROUPS[config[CONF_TYPE]]
    enabled_mask = 0
    for name in config[CONF_EVENTS]:
        enabled_mask |= event_map[name]
    enabled_wake_mask = 0
    for name, mask in WAKE_SOURCE_EVENTS.items():
        if name in config[CONF_EVENTS]:
            enabled_wake_mask |= mask
    publish_generic_wakeup = "wakeup" in config[CONF_EVENTS]
    var = cg.new_Pvariable(
        config[CONF_ID],
        parent,
        event_type,
        enabled_mask,
        enabled_wake_mask,
        publish_generic_wakeup,
    )
    await event.register_event(var, config, event_types=config[CONF_EVENTS])
    await cg.register_component(var, config)
