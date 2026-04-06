import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_PERCENT,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

from .. import CONF_SYSTA_BUS_ID, CONF_SYSTASOLAR_AQUA, SystaBus, systa_bus_ns

SystaSolar_Aqua = systa_bus_ns.class_("SystaSolarAquaSensor", cg.Component)

CONF_TEMPERATURE_TSA = "temperature_tsa"
CONF_TEMPERATURE_TSE = "temperature_tse"
CONF_TEMPERATURE_TWU = "temperature_twu"
CONF_TEMPERATURE_TW2 = "temperature_tw2"
CONF_PUMP_SPEED = "pump_speed"

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SYSTASOLAR_AQUA: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(SystaSolar_Aqua),
                cv.GenerateID(CONF_SYSTA_BUS_ID): cv.use_id(SystaBus),
                cv.Optional(CONF_TEMPERATURE_TSA): sensor.sensor_schema(
                    unit_of_measurement=UNIT_CELSIUS,
                    icon=ICON_THERMOMETER,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_TEMPERATURE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_TEMPERATURE_TSE): sensor.sensor_schema(
                    unit_of_measurement=UNIT_CELSIUS,
                    icon=ICON_THERMOMETER,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_TEMPERATURE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_TEMPERATURE_TWU): sensor.sensor_schema(
                    unit_of_measurement=UNIT_CELSIUS,
                    icon=ICON_THERMOMETER,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_TEMPERATURE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_TEMPERATURE_TW2): sensor.sensor_schema(
                    unit_of_measurement=UNIT_CELSIUS,
                    icon=ICON_THERMOMETER,
                    accuracy_decimals=1,
                    device_class=DEVICE_CLASS_TEMPERATURE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_PUMP_SPEED): sensor.sensor_schema(
                    unit_of_measurement=UNIT_PERCENT,
                    icon=ICON_PERCENT,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_EMPTY,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
            }
        ),
    },
    key=CONF_MODEL,
    lower=True,
    space="_",
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config[CONF_MODEL] == CONF_SYSTASOLAR_AQUA:
        if CONF_TEMPERATURE_TSA in config:
            sens = await sensor.new_sensor(config[CONF_TEMPERATURE_TSA])
            cg.add(var.set_temperature_tsa_sensor(sens))
        if CONF_TEMPERATURE_TSE in config:
            sens = await sensor.new_sensor(config[CONF_TEMPERATURE_TSE])
            cg.add(var.set_temperature_tse_sensor(sens))
        if CONF_TEMPERATURE_TWU in config:
            sens = await sensor.new_sensor(config[CONF_TEMPERATURE_TWU])
            cg.add(var.set_temperature_twu_sensor(sens))
        if CONF_TEMPERATURE_TW2 in config:
            sens = await sensor.new_sensor(config[CONF_TEMPERATURE_TW2])
            cg.add(var.set_temperature_tw2_sensor(sens))
        if CONF_PUMP_SPEED in config:
            sens = await sensor.new_sensor(config[CONF_PUMP_SPEED])
            cg.add(var.set_pump_speed_sensor(sens))

    systa_bus = await cg.get_variable(config[CONF_SYSTA_BUS_ID])
    cg.add(systa_bus.register_listener(var))
