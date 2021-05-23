import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_SENSORS
from .. import custom_ns

CustomSensorConstructor = custom_ns.class_("CustomSensorConstructor")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomSensorConstructor),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
        cv.Required(CONF_SENSORS): cv.ensure_list(sensor.SENSOR_SCHEMA),
    }
)


async def to_code(config):
    template_ = await cg.process_lambda(
        config[CONF_LAMBDA], [], return_type=cg.std_vector.template(sensor.SensorPtr)
    )

    rhs = CustomSensorConstructor(template_)
    var = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_SENSORS]):
        sens = cg.Pvariable(conf[CONF_ID], var.get_sensor(i))
        await sensor.register_sensor(sens, conf)
