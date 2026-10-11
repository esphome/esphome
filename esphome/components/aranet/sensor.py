import esphome.codegen as cg
from esphome.components import ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_PRESSURE,
    CONF_RADON,
    CONF_SIGNAL_STRENGTH,
    CONF_TEMPERATURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_RADON,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RADIOACTIVE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_BECQUEREL_PER_CUBIC_METER,
    UNIT_CELSIUS,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_HECTOPASCAL,
    UNIT_MICROSILVERTS_PER_HOUR,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    UNIT_SECOND,
)
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base"]

CONF_MEASUREMENT_AGE = "measurement_age"
CONF_MEASUREMENT_INTERVAL = "measurement_interval"
CONF_RADIATION_RATE = "radiation_rate"
CONF_RADIATION_TOTAL = "radiation_total"
CONF_RADIATION_DURATION = "radiation_duration"
UNIT_NANOSIEVERT = "nSv"

aranet_ns = cg.esphome_ns.namespace("aranet")
Aranet = aranet_ns.class_("Aranet", ble_device_base.ESPBTDeviceListener, cg.Component)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("aranet"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Aranet),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MEASUREMENT_INTERVAL): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MEASUREMENT_AGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_SIGNAL_STRENGTH): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_RADON): sensor.sensor_schema(
                unit_of_measurement=UNIT_BECQUEREL_PER_CUBIC_METER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_RADON,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RADIATION_RATE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROSILVERTS_PER_HOUR,
                accuracy_decimals=3,
                icon=ICON_RADIOACTIVE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RADIATION_TOTAL): sensor.sensor_schema(
                unit_of_measurement=UNIT_NANOSIEVERT,
                accuracy_decimals=0,
                icon=ICON_RADIOACTIVE,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_RADIATION_DURATION): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_MAC_ADDRESS].as_hex)
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    for key, setter in (
        (CONF_CO2, var.set_co2_sensor),
        (CONF_TEMPERATURE, var.set_temperature_sensor),
        (CONF_HUMIDITY, var.set_humidity_sensor),
        (CONF_PRESSURE, var.set_pressure_sensor),
        (CONF_BATTERY_LEVEL, var.set_battery_level_sensor),
        (CONF_MEASUREMENT_INTERVAL, var.set_measurement_interval_sensor),
        (CONF_MEASUREMENT_AGE, var.set_measurement_age_sensor),
        (CONF_SIGNAL_STRENGTH, var.set_signal_strength_sensor),
        (CONF_RADON, var.set_radon_sensor),
        (CONF_RADIATION_RATE, var.set_radiation_rate_sensor),
        (CONF_RADIATION_TOTAL, var.set_radiation_total_sensor),
        (CONF_RADIATION_DURATION, var.set_radiation_duration_sensor),
    ):
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(setter(sens))
