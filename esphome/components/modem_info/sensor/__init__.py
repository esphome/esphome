import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.modem import MODEM_COMPONENT_SCHEMA, modem_ns
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL,
    UNIT_PERCENT,
)

CODEOWNERS = ["@oarcher"]

AUTO_LOAD = []

DEPENDENCIES = ["modem"]

IS_PLATFORM_COMPONENT = True

CONF_BER = "ber"
CONF_RSSI = "rssi"

ICON_SIGNAL_BAR = "mdi:signal"

ModemSensor = modem_ns.class_("ModemSensor", cg.PollingComponent)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModemSensor),
            cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL,
                accuracy_decimals=0,
                icon=ICON_SIGNAL_BAR,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BER): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(MODEM_COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s")),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if rssi := config.get(CONF_RSSI, None):
        rssi_sensor = await sensor.new_sensor(rssi)
        cg.add(var.set_rssi_sensor(rssi_sensor))

    if ber := config.get(CONF_BER, None):
        ber_sensor = await sensor.new_sensor(ber)
        cg.add(var.set_ber_sensor(ber_sensor))

    await cg.register_component(var, config)
