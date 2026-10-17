import esphome.codegen as cg
from esphome.components import ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION,
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_BATTERY_VOLTAGE,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_MEASUREMENT_SEQUENCE_NUMBER,
    CONF_MOVEMENT_COUNTER,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    CONF_TX_POWER,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ACCELERATION,
    ICON_ACCELERATION_X,
    ICON_ACCELERATION_Y,
    ICON_ACCELERATION_Z,
    ICON_GAUGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_G,
    UNIT_HECTOPASCAL,
    UNIT_PERCENT,
    UNIT_VOLT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base", "ruuvi_ble"]

ruuvitag_ns = cg.esphome_ns.namespace("ruuvitag")
RuuviTag = ruuvitag_ns.class_(
    "RuuviTag", ble_device_base.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("ruuvitag"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RuuviTag),
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
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACCELERATION): sensor.sensor_schema(
                unit_of_measurement=UNIT_G,
                icon=ICON_ACCELERATION,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACCELERATION_X): sensor.sensor_schema(
                unit_of_measurement=UNIT_G,
                icon=ICON_ACCELERATION_X,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACCELERATION_Y): sensor.sensor_schema(
                unit_of_measurement=UNIT_G,
                icon=ICON_ACCELERATION_Y,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACCELERATION_Z): sensor.sensor_schema(
                unit_of_measurement=UNIT_G,
                icon=ICON_ACCELERATION_Z,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_TX_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MOVEMENT_COUNTER): sensor.sensor_schema(
                icon=ICON_GAUGE,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MEASUREMENT_SEQUENCE_NUMBER): sensor.sensor_schema(
                icon=ICON_GAUGE,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature)
    await sensors(CONF_HUMIDITY, var.set_humidity)
    await sensors(CONF_PRESSURE, var.set_pressure)
    await sensors(CONF_ACCELERATION, var.set_acceleration)
    await sensors(CONF_ACCELERATION_X, var.set_acceleration_x)
    await sensors(CONF_ACCELERATION_Y, var.set_acceleration_y)
    await sensors(CONF_ACCELERATION_Z, var.set_acceleration_z)
    await sensors(CONF_BATTERY_VOLTAGE, var.set_battery_voltage)
    await sensors(CONF_TX_POWER, var.set_tx_power)
    await sensors(CONF_MOVEMENT_COUNTER, var.set_movement_counter)
    await sensors(CONF_MEASUREMENT_SEQUENCE_NUMBER, var.set_measurement_sequence_number)
