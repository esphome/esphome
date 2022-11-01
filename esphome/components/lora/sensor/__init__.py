import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import lora
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    UNIT_DECIBEL,
    ICON_SIGNAL,
    DEVICE_CLASS_SIGNAL_STRENGTH,
)

CODEOWNERS = ["@SenexCrenshaw", "@alf-scotland"]
DEPENDENCIES = ["lora"]

LoraRSSISensor = lora.lora_ns.class_(
    "LoraRSSISensor", sensor.Sensor, cg.PollingComponent
)

LoraSNRSensor = lora.lora_ns.class_("LoraSNRSensor", sensor.Sensor, cg.PollingComponent)

CONF_RSSI = "rssi"
CONF_SNR = "snr"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(lora.CONF_LORA_ID): cv.use_id(lora.LoraComponent),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL,
            icon=ICON_SIGNAL,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        )
        .extend(
            {
                cv.GenerateID(): cv.declare_id(LoraRSSISensor),
            }
        )
        .extend(cv.polling_component_schema("60s")),
        cv.Optional(CONF_SNR): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL,
            icon=ICON_SIGNAL,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        )
        .extend(
            {
                cv.GenerateID(): cv.declare_id(LoraSNRSensor),
            }
        )
        .extend(cv.polling_component_schema("60s")),
    }
)


def to_code(config):
    parent = yield cg.get_variable(config[lora.CONF_LORA_ID])

    if CONF_RSSI in config:
        conf = config[CONF_RSSI]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield sensor.register_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(var.register_lora(parent))

    if CONF_SNR in config:
        conf = config[CONF_SNR]
        var = cg.new_Pvariable(conf[CONF_ID])
        yield sensor.register_sensor(var, conf)
        yield cg.register_component(var, conf)
        cg.add(var.register_lora(parent))
