from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_ID,
    CONF_MODEL,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_VOLT,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

# Constants that do not yet exist in esphome/const.py
CONF_BATTERY_CHARGE_RATE = "battery_charge_rate"
UNIT_PERCENT_PER_HOUR = "%/h"

MODEL_MAX17043 = "MAX17043"
MODEL_MAX17048 = "MAX17048"

max17043_ns = cg.esphome_ns.namespace("max17043")
MAX17043Component = max17043_ns.class_(
    "MAX17043Component", cg.PollingComponent, i2c.I2CDevice
)

MAX17043Model = max17043_ns.enum("MAX17043Model", is_class=True)
MODELS = {
    MODEL_MAX17043: MAX17043Model.MAX17043_MODEL_MAX17043,
    MODEL_MAX17048: MAX17043Model.MAX17043_MODEL_MAX17048,
}

# Actions
SleepAction = max17043_ns.class_("SleepAction", automation.Action)


def _validate_model_features(config):
    # The CRATE register (0x16) only exists on the MAX17048/MAX17049.
    if CONF_BATTERY_CHARGE_RATE in config and config[CONF_MODEL] != MODEL_MAX17048:
        raise cv.Invalid(
            f"{CONF_BATTERY_CHARGE_RATE} is only supported by model {MODEL_MAX17048}",
            path=[CONF_BATTERY_CHARGE_RATE],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MAX17043Component),
            cv.Optional(CONF_MODEL, default=MODEL_MAX17043): cv.enum(
                MODELS, upper=True
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_CHARGE_RATE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT_PER_HOUR,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x36)),
    _validate_model_features,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))

    if voltage_config := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(sens))

    if level_config := config.get(CONF_BATTERY_LEVEL):
        sens = await sensor.new_sensor(level_config)
        cg.add(var.set_battery_remaining_sensor(sens))

    if charge_rate_config := config.get(CONF_BATTERY_CHARGE_RATE):
        sens = await sensor.new_sensor(charge_rate_config)
        cg.add(var.set_charge_rate_sensor(sens))


MAX17043_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MAX17043Component),
    }
)


@automation.register_action(
    "max17043.sleep_mode", SleepAction, MAX17043_ACTION_SCHEMA, synchronous=True
)
async def max17043_sleep_mode_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
