import hashlib
from pathlib import Path
import urllib.parse

from esphome import external_files
import esphome.codegen as cg
from esphome.components import esp32, i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_IAQ_ACCURACY,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ICON_GAS_CYLINDER,
    ICON_GAUGE,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_OHM,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    Framework,
)
from esphome.core import CORE

DEPENDENCIES = ["i2c"]

DOMAIN = "bme690"

CONF_BSEC_LIBRARY = "bsec_library"
CONF_GAS_RESISTANCE = "gas_resistance"
CONF_IAQ = "iaq"
CONF_STATIC_IAQ = "static_iaq"
CONF_CO2_EQUIVALENT = "co2_equivalent"
CONF_BREATH_VOC_EQUIVALENT = "breath_voc_equivalent"
CONF_GAS_PERCENTAGE = "gas_percentage"
CONF_COMPENSATED_TEMPERATURE = "compensated_temperature"
CONF_COMPENSATED_HUMIDITY = "compensated_humidity"

UNIT_IAQ = "IAQ"

bme690_ns = cg.esphome_ns.namespace("bme690")
BME690Component = bme690_ns.class_(
    "BME690Component", cg.PollingComponent, i2c.I2CDevice
)


def _compute_local_file_path(url: str) -> Path:
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / f"bsec_{key}.a"


def _resolve_bsec_library(value: Path | str) -> Path:
    if isinstance(value, Path):
        return value

    parsed = urllib.parse.urlparse(value)
    if parsed.scheme == "file":
        file_path = Path(parsed.path)
        if not file_path.is_file():
            raise cv.Invalid(f"Could not find file '{file_path}'")
        return file_path

    path = _compute_local_file_path(value)
    external_files.download_content(value, path)
    return path

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BME690Component),
            cv.Required(CONF_BSEC_LIBRARY): cv.Any(cv.file_, cv.url),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
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
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_STATIC_IAQ): sensor.sensor_schema(
                unit_of_measurement=UNIT_IAQ,
                icon=ICON_GAUGE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CO2_EQUIVALENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BREATH_VOC_EQUIVALENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_GAS_PERCENTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMPENSATED_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMPENSATED_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x76)),
    cv.only_with_framework(
        frameworks=Framework.ESP_IDF,
        suggestions={
            Framework.ARDUINO: ("bme680", "bme68x_bsec2"),
        },
    ),
    cv.All(
        cv.only_on_esp32,
        esp32.only_on_variant(supported=[esp32.VARIANT_ESP32C6]),
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))
    if CONF_PRESSURE in config:
        sens = await sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(var.set_pressure_sensor(sens))
    if CONF_GAS_RESISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_GAS_RESISTANCE])
        cg.add(var.set_gas_resistance_sensor(sens))
    if CONF_IAQ in config:
        sens = await sensor.new_sensor(config[CONF_IAQ])
        cg.add(var.set_iaq_sensor(sens))
    if CONF_IAQ_ACCURACY in config:
        sens = await sensor.new_sensor(config[CONF_IAQ_ACCURACY])
        cg.add(var.set_iaq_accuracy_sensor(sens))
    if CONF_STATIC_IAQ in config:
        sens = await sensor.new_sensor(config[CONF_STATIC_IAQ])
        cg.add(var.set_static_iaq_sensor(sens))
    if CONF_CO2_EQUIVALENT in config:
        sens = await sensor.new_sensor(config[CONF_CO2_EQUIVALENT])
        cg.add(var.set_co2_equivalent_sensor(sens))
    if CONF_BREATH_VOC_EQUIVALENT in config:
        sens = await sensor.new_sensor(config[CONF_BREATH_VOC_EQUIVALENT])
        cg.add(var.set_breath_voc_equivalent_sensor(sens))
    if CONF_GAS_PERCENTAGE in config:
        sens = await sensor.new_sensor(config[CONF_GAS_PERCENTAGE])
        cg.add(var.set_gas_percentage_sensor(sens))
    if CONF_COMPENSATED_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_COMPENSATED_TEMPERATURE])
        cg.add(var.set_comp_temperature_sensor(sens))
    if CONF_COMPENSATED_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_COMPENSATED_HUMIDITY])
        cg.add(var.set_comp_humidity_sensor(sens))

    lib_path = _resolve_bsec_library(config[CONF_BSEC_LIBRARY])
    esp32.add_extra_build_file("libalgobsec.a", lib_path)

    build_dir = CORE.relative_build_path()
    cg.add_build_flag(
        f"-L{build_dir} -Wl,--whole-archive -lalgobsec -Wl,--no-whole-archive"
    )
