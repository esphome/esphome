import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import uart, text_sensor
from esphome.const import CONF_ID

CODEOWNERS = ["lorki97", "florianL21"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "text_sensor"]

mr24hpb1_ns = cg.esphome_ns.namespace("mr24hpb1")

CONF_REQ_KEY = "req_key"

CONF_PIR = "pir"
CONF_DEVICE_ID = "device_id"
CONF_SOFTWARE_VERSION = "software_version"

MR24HPB1Component = mr24hpb1_ns.class_(
    "MR24HPB1Component", cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MR24HPB1Component),
        # cv.Optional(CONF_PIR): binary_sensor.binary_sensor_schema(
        #     MR24D11C10Component,
        #     device_class=DEVICE_CLASS_MOTION,
        #     icon=ICON_MOTION_SENSOR,
        # ),
        cv.Optional(CONF_DEVICE_ID): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_SOFTWARE_VERSION): text_sensor.text_sensor_schema(),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    # var = await binary_sensor.new_binary_sensor(config)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_DEVICE_ID in config:
        sens = await text_sensor.new_text_sensor(config[CONF_DEVICE_ID])
        cg.add(var.set_device_id_sensor(sens))

    if CONF_SOFTWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SOFTWARE_VERSION])
        cg.add(var.set_software_version_sensor(sens))
