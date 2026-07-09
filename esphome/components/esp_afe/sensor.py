import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_ESP_AFE_ID, EspAfe, esp_afe_ns

DEPENDENCIES = ["esp_afe"]

CONF_INPUT_VOLUME = "input_volume"
CONF_OUTPUT_RMS = "output_rms"

AfeInputVolumeSensor = esp_afe_ns.class_(
    "AfeInputVolumeSensor",
    sensor.Sensor,
    cg.PollingComponent,
    cg.Parented.template(EspAfe),
)

AfeOutputRmsSensor = esp_afe_ns.class_(
    "AfeOutputRmsSensor",
    sensor.Sensor,
    cg.PollingComponent,
    cg.Parented.template(EspAfe),
)


def _sensor_schema(sensor_class, icon):
    return sensor.sensor_schema(
        sensor_class,
        unit_of_measurement="dBFS",
        accuracy_decimals=1,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=icon,
    ).extend(cv.polling_component_schema("250ms"))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESP_AFE_ID): cv.use_id(EspAfe),
        cv.Optional(CONF_INPUT_VOLUME): _sensor_schema(
            AfeInputVolumeSensor, "mdi:microphone-message"
        ),
        cv.Optional(CONF_OUTPUT_RMS): _sensor_schema(
            AfeOutputRmsSensor, "mdi:waveform"
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ESP_AFE_ID])

    if CONF_INPUT_VOLUME in config:
        cg.add(parent.set_input_volume_sensor_enabled(True))
        var = await sensor.new_sensor(config[CONF_INPUT_VOLUME])
        await cg.register_component(var, config[CONF_INPUT_VOLUME])
        cg.add(var.set_parent(parent))

    if CONF_OUTPUT_RMS in config:
        cg.add(parent.set_output_rms_sensor_enabled(True))
        var = await sensor.new_sensor(config[CONF_OUTPUT_RMS])
        await cg.register_component(var, config[CONF_OUTPUT_RMS])
        cg.add(var.set_parent(parent))
