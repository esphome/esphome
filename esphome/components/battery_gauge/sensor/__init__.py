import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CAPACITY,
    CONF_INITIAL_STATE,
    DEVICE_CLASS_BATTERY,
    ICON_BATTERY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

from .. import battery_gauge_ns

BatteryGaugeSensor = battery_gauge_ns.class_(
    "BatteryGaugeSensor", sensor.Sensor, cg.Component
)

CONF_CURRENT_SOURCE = "current_source"
CONF_EFFICIENCY = "efficiency"
CONF_MAX_CHARGE_VOLTAGE = "max_charge_voltage"
CONF_VOLTAGE_SOURCE = "voltage_source"


capacity_ah = cv.All(
    cv.float_with_unit("capacity", "(ah|AH|Ah|aH)?"),
    cv.float_range(min=0, min_included=False),
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        BatteryGaugeSensor,
        unit_of_measurement=UNIT_PERCENT,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_BATTERY,
        accuracy_decimals=1,
        icon=ICON_BATTERY,
    )
    .extend(
        {
            cv.Required(CONF_VOLTAGE_SOURCE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CURRENT_SOURCE): cv.use_id(sensor.Sensor),
            cv.Required(CONF_CAPACITY): capacity_ah,
            cv.Optional(CONF_EFFICIENCY, default=0.98): cv.percentage,
            cv.Required(CONF_MAX_CHARGE_VOLTAGE): cv.voltage,
            cv.Optional(CONF_INITIAL_STATE): cv.All(
                cv.percentage, cv.Range(min=0, max=1.0)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    voltage_source = await cg.get_variable(config[CONF_VOLTAGE_SOURCE])
    current_source = await cg.get_variable(config[CONF_CURRENT_SOURCE])
    capacity = config[CONF_CAPACITY]
    efficiency = config[CONF_EFFICIENCY]
    var = await sensor.new_sensor(
        config,
        voltage_source,
        current_source,
        capacity,
        efficiency,
        config[CONF_MAX_CHARGE_VOLTAGE],
    )
    if initial_state := config.get(CONF_INITIAL_STATE):
        cg.add(var.set_initial_state(initial_state))
    await cg.register_component(var, config)
