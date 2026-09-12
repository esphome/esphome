import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_MODEL

DEPENDENCIES = ["uart"]

CODEOWNERS = ["@mikelawrence"]
MULTI_CONF = True

CONF_DFROBOT_C4001_ID = "dfrobot_c4001_id"

dfrobot_c4001_ns = cg.esphome_ns.namespace("dfrobot_c4001")
DFRobotC4001Hub = dfrobot_c4001_ns.class_(
    "DFRobotC4001Hub", cg.Component, uart.UARTDevice
)

ModeConfig = dfrobot_c4001_ns.enum("ModeConfig")
ModelConfig = dfrobot_c4001_ns.enum("ModelConfig")

CONF_MODE_ENUM = {
    "PRESENCE": ModeConfig.MODE_PRESENCE,
    "SPEED_AND_DISTANCE": ModeConfig.MODE_SPEED_AND_DISTANCE,
}

CONF_MODEL_ENUM = {
    "SEN0609": ModelConfig.MODEL_SEN0609,
    "SEN0610": ModelConfig.MODEL_SEN0610,
}


HUB_CHILD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DFROBOT_C4001_ID): cv.use_id(DFRobotC4001Hub),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DFRobotC4001Hub),
            cv.Required(CONF_MODE): cv.enum(CONF_MODE_ENUM, upper=True, space="_"),
            cv.Required(CONF_MODEL): cv.enum(CONF_MODEL_ENUM, upper=True, space="_"),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "dfrobot_c4001",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_model(config[CONF_MODEL]))
