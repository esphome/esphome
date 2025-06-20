import esphome.codegen as cg
from esphome.components import iwr6843aop, number, sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@your-github-username"]

iwr6843aop_ns = cg.esphome_ns.namespace("iwr6843aop")
IWR6843AOPComponent = iwr6843aop_ns.class_(
    "IWR6843AOPComponent", cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IWR6843AOPComponent),
        cv.GenerateID("uart1_id"): cv.use_id(uart.UARTComponent),
        cv.GenerateID("uart2_id"): cv.use_id(uart.UARTComponent),
        cv.Optional("float_input", default={}): cv.Schema(
            {
                cv.Optional("corner_1_x"): cv.use_id(number.Number),
                cv.Optional("corner_1_y"): cv.use_id(number.Number),
                cv.Optional("corner_2_x"): cv.use_id(number.Number),
                cv.Optional("corner_2_y"): cv.use_id(number.Number),
            }
        ),
        cv.Optional("float_output"): sensor.sensor_schema(),
        cv.Optional("int_output"): sensor.sensor_schema(),
    }
)


async def to_code(config):
    uart1 = await cg.get_variable(config["uart1_id"])
    uart2 = await cg.get_variable(config["uart2_id"])
    var = cg.new_Pvariable(config[CONF_ID], uart1, uart2)
    await cg.register_component(var, config)
    float_input = config.get("float_input", {})
    for key in ["corner_1_x", "corner_1_y", "corner_2_x", "corner_2_y"]:
        if key in float_input:
            sens = await cg.get_variable(float_input[key])
            cg.add(var.set_float_input(key, sens))
