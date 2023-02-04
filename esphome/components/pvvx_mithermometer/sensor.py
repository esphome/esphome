import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, esp32_ble_tracker
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_MAC_ADDRESS,
    CONF_HUMIDITY,
    CONF_SIGNAL_STRENGTH,
    CONF_TEMPERATURE,
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_PERCENT,
    UNIT_VOLT,
)

CODEOWNERS = ["@pasiz"]

AUTO_LOAD = ["binary_sensor", "esp32_ble_tracker"]

pvvx_mithermometer_ns = cg.esphome_ns.namespace("pvvx_mithermometer")
PVVXMiThermometer = pvvx_mithermometer_ns.class_(
    "PVVXMiThermometer", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONF_RDS_INPUT = "rds_input"
CONF_TRG_OUTPUT = "trg_output"
CONF_TRIGGER_ON = "trigger_on"
CONF_HUMI_OUT_ON = "humi_out_on"
CONF_TEMP_OUT_ON = "temp_out_on"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PVVXMiThermometer),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_RDS_INPUT): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_TRG_OUTPUT): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_TRIGGER_ON): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_HUMI_OUT_ON): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_TEMP_OUT_ON): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_SIGNAL_STRENGTH): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
    if CONF_BATTERY_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_VOLTAGE])
        cg.add(var.set_battery_voltage(sens))
    if CONF_RDS_INPUT in config:
        bs = await binary_sensor.new_binary_sensor(config[CONF_RDS_INPUT])
        cg.add(var.set_rds_input(bs))
    if CONF_TRG_OUTPUT in config:
        bs = await binary_sensor.new_binary_sensor(config[CONF_TRG_OUTPUT])
        cg.add(var.set_trg_output(bs))
    if CONF_TRIGGER_ON in config:
        bs = await binary_sensor.new_binary_sensor(config[CONF_TRIGGER_ON])
        cg.add(var.set_trigger_on(bs))
    if CONF_HUMI_OUT_ON in config:
        bs = await binary_sensor.new_binary_sensor(config[CONF_HUMI_OUT_ON])
        cg.add(var.set_humi_out_on(bs))
    if CONF_TEMP_OUT_ON in config:
        bs = await binary_sensor.new_binary_sensor(config[CONF_TEMP_OUT_ON])
        cg.add(var.set_temp_out_on(bs))
    if CONF_SIGNAL_STRENGTH in config:
        sens = await sensor.new_sensor(config[CONF_SIGNAL_STRENGTH])
        cg.add(var.set_signal_strength(sens))
