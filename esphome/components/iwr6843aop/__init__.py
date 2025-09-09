import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, uart, binary_sensor
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
        cv.Optional("enabled", default=False): cv.boolean,  # Add enabled flag
        cv.Optional("float_input", default={}): cv.Schema(
            {
                cv.Optional("width"): cv.use_id(number.Number),
                cv.Optional("length"): cv.use_id(number.Number),
            }
        ),
        cv.Optional("binary_sensor", default={}): cv.Schema(
            {
                cv.Optional("room_presence"): cv.use_id(binary_sensor.BinarySensor),
                cv.Optional("bed_presence"): cv.use_id(binary_sensor.BinarySensor),
            }
        ),
    }
)


async def to_code(config):
    # Only create and register the component if enabled is True
    if config.get("enabled", False):
        uart1 = await cg.get_variable(config["uart1_id"])
        uart2 = await cg.get_variable(config["uart2_id"])
        var = cg.new_Pvariable(config[CONF_ID], uart1, uart2)
        await cg.register_component(var, config)
        
        float_input = config.get("float_input", {})
        
        for key in ["width", "length"]:
            if key in float_input:
                sens = await cg.get_variable(float_input[key])
                cg.add(var.set_float_input(key, sens))
                
        binary_sensor_cfg = config.get("binary_sensor", {})
        for key in ["room_presence", "bed_presence"]:
            if key in binary_sensor_cfg:
                sens = await cg.get_variable(binary_sensor_cfg[key])
                cg.add(var.set_binary_sensor(key, sens))
    else:
        # Create a dummy variable to satisfy config requirements but don't register it
        var = cg.new_Pvariable(config[CONF_ID])
