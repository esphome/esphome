import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client

from esphome.const import (
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_CELSIUS,
    CONF_ID,
    CONF_HUMIDITY,
    CONF_TVOC,
    CONF_TEMPERATURE,
    UNIT_PARTS_PER_BILLION,
    ICON_RADIATOR,
)

DEPENDENCIES = ["ble_client"]

airthings_wave_mini_ns = cg.esphome_ns.namespace("airthings_wave_mini")
AirthingsWaveMini = airthings_wave_mini_ns.class_(
    "AirthingsWaveMini", cg.PollingComponent, ble_client.BLEClientNode
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AirthingsWaveMini),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("5mins"))
    .extend(ble_client.BLE_CLIENT_SCHEMA),
    # Until BLEUUID reference removed
    cv.only_with_arduino,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await ble_client.register_ble_node(var, config)

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
    if CONF_TVOC in config:
        sens = await sensor.new_sensor(config[CONF_TVOC])
        cg.add(var.set_tvoc(sens))
