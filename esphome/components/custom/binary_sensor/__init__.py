import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_BINARY_SENSORS, CONF_ID, CONF_LAMBDA
from .. import custom_ns

CustomBinarySensorConstructor = custom_ns.class_("CustomBinarySensorConstructor")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomBinarySensorConstructor),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
        cv.Required(CONF_BINARY_SENSORS): cv.ensure_list(
            binary_sensor.BINARY_SENSOR_SCHEMA
        ),
    }
)


async def to_code(config):
    template_ = await cg.process_lambda(
        config[CONF_LAMBDA],
        [],
        return_type=cg.std_vector.template(binary_sensor.BinarySensorPtr),
    )

    rhs = CustomBinarySensorConstructor(template_)
    custom = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_BINARY_SENSORS]):
        rhs = custom.Pget_binary_sensor(i)
        await binary_sensor.register_binary_sensor(rhs, conf)
