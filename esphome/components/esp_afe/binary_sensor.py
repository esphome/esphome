import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SOUND, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_ESP_AFE_ID, EspAfe, esp_afe_ns

DEPENDENCIES = ["esp_afe"]

CONF_VAD = "vad"

AfeVadBinarySensor = esp_afe_ns.class_(
    "AfeVadBinarySensor",
    binary_sensor.BinarySensor,
    cg.PollingComponent,
    cg.Parented.template(EspAfe),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESP_AFE_ID): cv.use_id(EspAfe),
        cv.Required(CONF_VAD): binary_sensor.binary_sensor_schema(
            AfeVadBinarySensor,
            device_class=DEVICE_CLASS_SOUND,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:account-voice",
        ).extend(cv.polling_component_schema("100ms")),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ESP_AFE_ID])
    conf = config[CONF_VAD]
    var = await binary_sensor.new_binary_sensor(conf)
    await cg.register_component(var, conf)
    cg.add(var.set_parent(parent))
