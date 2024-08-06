import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.modem import final_validate_platform
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_EMPTY

CODEOWNERS = ["@oarcher"]

AUTO_LOAD = []

DEPENDENCIES = ["modem"]

# MULTI_CONF = True
IS_PLATFORM_COMPONENT = True

CONF_NETWORK_TYPE = "network_type"

modem_text_sensor_ns = cg.esphome_ns.namespace("modem_text_sensor")
ModemTextSensorComponent = modem_text_sensor_ns.class_(
    "ModemTextSensor", cg.PollingComponent
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModemTextSensorComponent),
            cv.Optional(CONF_NETWORK_TYPE): text_sensor.text_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
            ),
        }
    ).extend(cv.polling_component_schema("60s"))
)


FINAL_VALIDATE_SCHEMA = final_validate_platform


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if network_type := config.get(CONF_NETWORK_TYPE, None):
        network_type_text_sensor = await text_sensor.new_text_sensor(network_type)
        cg.add(var.set_network_type_text_sensor(network_type_text_sensor))

    await cg.register_component(var, config)
