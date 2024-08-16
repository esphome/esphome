import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SIGNAL_STRENGTH, DEVICE_CLASS_EMPTY

from .. import MODEM_COMPONENT_SCHEMA, final_validate_platform, modem_ns

CODEOWNERS = ["@oarcher"]

AUTO_LOAD = []

DEPENDENCIES = ["modem"]

IS_PLATFORM_COMPONENT = True

CONF_NETWORK_TYPE = "network_type"

ModemTextSensor = modem_ns.class_("ModemTextSensor", cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModemTextSensor),
            cv.Optional(CONF_NETWORK_TYPE): text_sensor.text_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_SIGNAL_STRENGTH): text_sensor.text_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
                icon="mdi:signal",
            ),
        }
    )
    .extend(MODEM_COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


FINAL_VALIDATE_SCHEMA = final_validate_platform


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if network_type := config.get(CONF_NETWORK_TYPE, None):
        network_type_text_sensor = await text_sensor.new_text_sensor(network_type)
        cg.add(var.set_network_type_text_sensor(network_type_text_sensor))

    if signal_strength := config.get(CONF_SIGNAL_STRENGTH, None):
        signal_strength_text_sensor = await text_sensor.new_text_sensor(signal_strength)
        cg.add(var.set_signal_strength_text_sensor(signal_strength_text_sensor))

    await cg.register_component(var, config)
