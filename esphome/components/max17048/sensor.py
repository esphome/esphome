from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_ID,
    CONF_RATE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["max170xx"]
DEPENDENCIES = ["i2c"]

UNIT_PERCENT_PER_HOUR = "%/h"

max17048_ns = cg.esphome_ns.namespace("max17048")
MAX17048Component = max17048_ns.class_(
    "MAX17048Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MAX17048Component),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE,
                accuracy_decimals=3,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                kind=sensor.BATTERY,
                accuracy_decimals=2,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_RATE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT_PER_HOUR,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x36))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_BATTERY_VOLTAGE, var.set_battery_voltage_sensor)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level_sensor)
    await sensors(CONF_RATE, var.set_rate_sensor)


MAX17048_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MAX17048Component),
    }
)


automation.register_apply_action(
    "max17048.sleep_mode", MAX17048_ACTION_SCHEMA, automation.ApplyCall("sleep_mode()")
)
