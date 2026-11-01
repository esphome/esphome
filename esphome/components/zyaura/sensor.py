from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOCK_PIN,
    CONF_CO2,
    CONF_DATA_PIN,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    ICON_MOLECULE_CO2,
)
from esphome.types import ConfigType

zyaura_ns = cg.esphome_ns.namespace("zyaura")
ZyAuraSensor = zyaura_ns.class_("ZyAuraSensor", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ZyAuraSensor),
        cv.Required(CONF_CLOCK_PIN): cv.All(pins.internal_gpio_input_pin_schema),
        cv.Required(CONF_DATA_PIN): cv.All(pins.internal_gpio_input_pin_schema),
        cv.Optional(CONF_CO2): sensor.sensor_schema(
            kind=sensor.CARBON_DIOXIDE, icon=ICON_MOLECULE_CO2
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(kind=sensor.TEMPERATURE),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(kind=sensor.HUMIDITY),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.add_pin(var.set_pin_clock, config[CONF_CLOCK_PIN])
    await cg.add_pin(var.set_pin_data, config[CONF_DATA_PIN])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_CO2, var.set_co2_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
