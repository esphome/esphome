import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    DEVICE_CLASS_TEMPERATURE,
    ICON_PERCENT,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import CONF_SYSTA_BUS_ID, SystaBus, register_systa_bus_listener, systa_bus_ns

SystaSolarAquaSensor = systa_bus_ns.class_("SystaSolarAquaSensor", cg.Component)

CONF_SYSTASOLAR_AQUA = "systasolar_aqua"
CONF_TEMPERATURE_TSA = "temperature_tsa"
CONF_TEMPERATURE_TSE = "temperature_tse"
CONF_TEMPERATURE_TWU = "temperature_twu"
CONF_TEMPERATURE_TW2 = "temperature_tw2"
CONF_PUMP_SPEED = "pump_speed"

_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    icon=ICON_THERMOMETER,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)
_PUMP_SPEED_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    icon=ICON_PERCENT,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

# Sensors per model, keyed by config key; the C++ setter is "set_<key>_sensor"
MODEL_SENSORS = {
    CONF_SYSTASOLAR_AQUA: {
        CONF_TEMPERATURE_TSA: _TEMPERATURE_SCHEMA,
        CONF_TEMPERATURE_TSE: _TEMPERATURE_SCHEMA,
        CONF_TEMPERATURE_TWU: _TEMPERATURE_SCHEMA,
        CONF_TEMPERATURE_TW2: _TEMPERATURE_SCHEMA,
        CONF_PUMP_SPEED: _PUMP_SPEED_SCHEMA,
    },
}

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SYSTASOLAR_AQUA: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(SystaSolarAquaSensor),
                cv.GenerateID(CONF_SYSTA_BUS_ID): cv.use_id(SystaBus),
                **{
                    cv.Optional(key): schema
                    for key, schema in MODEL_SENSORS[CONF_SYSTASOLAR_AQUA].items()
                },
            }
        ),
    },
    key=CONF_MODEL,
    lower=True,
    space="_",
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for key in MODEL_SENSORS[config[CONF_MODEL]]:
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

    systa_bus = await cg.get_variable(config[CONF_SYSTA_BUS_ID])
    await register_systa_bus_listener(systa_bus, var)
