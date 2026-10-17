import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_PIN,
    CONF_TYPE,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from esphome.types import ConfigType

from .. import BASE_SCHEMA, CONF_M5PM1_ID, M5PM1_GPIO_OUTPUT_PIN_SCHEMA, m5pm1_ns

DEPENDENCIES = ["m5pm1"]

CONF_AW8737A = "aw8737a"
CONF_PULSE_NUM = "pulse_num"

AW8737ASwitch = m5pm1_ns.class_("AW8737ASwitch", cg.Component, switch.Switch)
M5PM1PowerSwitch = m5pm1_ns.class_("M5PM1PowerSwitch", cg.Component, switch.Switch)

AW8737APinMode = m5pm1_ns.enum("AW8737APinMode", is_class=True)
AW8737A_PIN_MODES = {
    "PUSH_PULL": AW8737APinMode.PUSH_PULL,
    "OPEN_DRAIN": AW8737APinMode.OPEN_DRAIN,
}

M5PM1PowerSwitchType = m5pm1_ns.enum("M5PM1PowerSwitchType", is_class=True)
POWER_SWITCH_TYPES = {
    "charger": M5PM1PowerSwitchType.M5PM1_POWER_SWITCH_TYPE_CHARGER,
    "dcdc_3v3": M5PM1PowerSwitchType.M5PM1_POWER_SWITCH_TYPE_DCDC_3V3,
    "ldo_3v3": M5PM1PowerSwitchType.M5PM1_POWER_SWITCH_TYPE_LDO_3V3,
    "boost_5v": M5PM1PowerSwitchType.M5PM1_POWER_SWITCH_TYPE_BOOST_5V,
    "hold_ldo_3v3": M5PM1PowerSwitchType.M5PM1_POWER_SWITCH_TYPE_HOLD_LDO_3V3,
    "hold_boost_5v": M5PM1PowerSwitchType.M5PM1_POWER_SWITCH_TYPE_HOLD_BOOST_5V,
    "hold_gpio": M5PM1PowerSwitchType.M5PM1_POWER_SWITCH_TYPE_HOLD_GPIO,
}

AW8737A_SCHEMA = (
    switch.switch_schema(
        AW8737ASwitch,
        device_class=DEVICE_CLASS_SWITCH,
        icon=ICON_PULSE,
        default_restore_mode="RESTORE_DEFAULT_OFF",
    )
    .extend(
        {
            cv.Required(CONF_PIN): M5PM1_GPIO_OUTPUT_PIN_SCHEMA,
            cv.Required(CONF_PULSE_NUM): cv.int_range(min=0, max=3),
        }
    )
    .extend(BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

POWER_SWITCH_SCHEMA = (
    switch.switch_schema(
        M5PM1PowerSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        default_restore_mode="DISABLED",
    )
    .extend(BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

POWER_CONFIG_SWITCH_SCHEMA = (
    switch.switch_schema(
        M5PM1PowerSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        default_restore_mode="DISABLED",
    )
    .extend(BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

GPIO_HOLD_SWITCH_SCHEMA = POWER_CONFIG_SWITCH_SCHEMA.extend(
    {cv.Required(CONF_PIN): cv.int_range(min=0, max=4)}
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_AW8737A: AW8737A_SCHEMA,
        "charger": POWER_CONFIG_SWITCH_SCHEMA,
        "dcdc_3v3": POWER_SWITCH_SCHEMA,
        "ldo_3v3": POWER_SWITCH_SCHEMA,
        "boost_5v": POWER_SWITCH_SCHEMA,
        "hold_ldo_3v3": POWER_CONFIG_SWITCH_SCHEMA,
        "hold_boost_5v": POWER_CONFIG_SWITCH_SCHEMA,
        "hold_gpio": GPIO_HOLD_SWITCH_SCHEMA,
    },
    default_type=CONF_AW8737A,
)


async def to_code(config: ConfigType) -> None:
    if config[CONF_TYPE] == CONF_AW8737A:
        var = await switch.new_switch(config)
        await cg.register_component(var, config)
        await cg.register_parented(var, config[CONF_M5PM1_ID])

        cg.add(var.set_pin(await cg.gpio_pin_expression(config[CONF_PIN])))
        cg.add(var.set_pulse_num(config[CONF_PULSE_NUM]))
        return

    parent = await cg.get_variable(config[CONF_M5PM1_ID])
    type_ = POWER_SWITCH_TYPES[config[CONF_TYPE]]
    if config[CONF_TYPE] == "hold_gpio":
        var = await switch.new_switch(config, parent, type_, config[CONF_PIN])
    else:
        var = await switch.new_switch(config, parent, type_)
    await cg.register_component(var, config)
