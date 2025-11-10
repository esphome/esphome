import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import water_heater, sensor
from esphome.const import CONF_ID, CONF_SENSOR, CONF_ECO_MODE

thermostat_ns = cg.esphome_ns.namespace("thermostat")
ThermostatWaterHeater = thermostat_ns.class_("ThermostatWaterHeater", water_heater.WaterHeater)

CONF_SUPPORTS_ECO = "supports_eco"

CONFIG_SCHEMA = (
    water_heater.WATER_HEATER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ThermostatWaterHeater),
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_SUPPORTS_ECO, default=False): cv.boolean,
        }
    )
    .extend({})  # future flags if needed
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await water_heater.register_water_heater(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))

    if config[CONF_SUPPORTS_ECO]:
        cg.add(var.set_supports_eco(True))
