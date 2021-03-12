import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    UNIT_DECIBEL,
    ICON_SIGNAL,
    DEVICE_CLASS_SIGNAL_STRENGTH,
)
from esphome.components import lora

DEPENDENCIES = ["lora"]

LoraRSSISensor = lora.lora_ns.class_(
    "LoraRSSISensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(UNIT_DECIBEL, ICON_SIGNAL, 2, DEVICE_CLASS_SIGNAL_STRENGTH)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(LoraRSSISensor),
            cv.GenerateID(lora.CONF_LORA_ID): cv.use_id(lora.LoraComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
    parent = yield cg.get_variable(config[lora.CONF_LORA_ID])
    cg.add(var.register_lora(parent))
