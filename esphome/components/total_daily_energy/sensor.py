import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, time
from esphome.const import CONF_ID, CONF_TIME_ID

DEPENDENCIES = ["time"]

CONF_POWER_ID = "power_id"
CONF_MIN_SAVE_INTERVAL = "min_save_interval"
total_daily_energy_ns = cg.esphome_ns.namespace("total_daily_energy")
TotalDailyEnergy = total_daily_energy_ns.class_(
    "TotalDailyEnergy", sensor.Sensor, cg.Component
)

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TotalDailyEnergy),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Required(CONF_POWER_ID): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_MIN_SAVE_INTERVAL): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    sens = yield cg.get_variable(config[CONF_POWER_ID])
    cg.add(var.set_parent(sens))
    time_ = yield cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
    if CONF_MIN_SAVE_INTERVAL in config:
        cg.add(var.set_min_save_interval(config[CONF_MIN_SAVE_INTERVAL]))
