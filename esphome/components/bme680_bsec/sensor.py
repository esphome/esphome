import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_GAS_RESISTANCE,
    CONF_HUMIDITY,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_OHM,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    ICON_GAS_CYLINDER,
    ICON_GAUGE,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
)
from . import (
    BME680BSECComponent,
    CONF_BME680_BSEC_ID,
    CONF_SAMPLE_RATE,
    SAMPLE_RATE_OPTIONS,
)

DEPENDENCIES = ["bme680_bsec"]

CONF_IAQ = "iaq"
CONF_IAQ_ACCURACY = "iaq_accuracy"
CONF_CO2_EQUIVALENT = "co2_equivalent"
CONF_BREATH_VOC_EQUIVALENT = "breath_voc_equivalent"
UNIT_IAQ = "IAQ"
ICON_ACCURACY = "mdi:checkbox-marked-circle-outline"
ICON_TEST_TUBE = "mdi:test-tube"

TYPES = [
    CONF_TEMPERATURE,
    CONF_PRESSURE,
    CONF_HUMIDITY,
    CONF_GAS_RESISTANCE,
    CONF_IAQ,
    CONF_IAQ_ACCURACY,
    CONF_CO2_EQUIVALENT,
    CONF_BREATH_VOC_EQUIVALENT,
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BME680_BSEC_ID): cv.use_id(BME680BSECComponent),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {cv.Optional(CONF_SAMPLE_RATE): cv.enum(SAMPLE_RATE_OPTIONS, upper=True)}
        ),
        cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_HECTOPASCAL,
            icon=ICON_GAUGE,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_PRESSURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {cv.Optional(CONF_SAMPLE_RATE): cv.enum(SAMPLE_RATE_OPTIONS, upper=True)}
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_WATER_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend(
            {cv.Optional(CONF_SAMPLE_RATE): cv.enum(SAMPLE_RATE_OPTIONS, upper=True)}
        ),
        cv.Optional(CONF_GAS_RESISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_OHM,
            icon=ICON_GAS_CYLINDER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IAQ): sensor.sensor_schema(
            unit_of_measurement=UNIT_IAQ,
            icon=ICON_GAUGE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IAQ_ACCURACY): sensor.sensor_schema(
            icon=ICON_ACCURACY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CO2_EQUIVALENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            icon=ICON_TEST_TUBE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BREATH_VOC_EQUIVALENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            icon=ICON_TEST_TUBE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def setup_conf(config, key, hub):
    if key in config:
        conf = config[key]
        sens = await sensor.new_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))
        if CONF_SAMPLE_RATE in conf:
            cg.add(getattr(hub, f"set_{key}_sample_rate")(conf[CONF_SAMPLE_RATE]))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BME680_BSEC_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
