import esphome.codegen as cg
from esphome.components import esp32, i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SAMPLE_RATE, CONF_TEMPERATURE_OFFSET

CODEOWNERS = ["@trvrnrth"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor"]
MULTI_CONF = True

CONF_BME680_BSEC_ID = "bme680_bsec_id"
CONF_IAQ_MODE = "iaq_mode"
CONF_SUPPLY_VOLTAGE = "supply_voltage"
CONF_STATE_SAVE_INTERVAL = "state_save_interval"

bme680_bsec_ns = cg.esphome_ns.namespace("bme680_bsec")

IAQMode = bme680_bsec_ns.enum("IAQMode")
IAQ_MODE_OPTIONS = {
    "STATIC": IAQMode.IAQ_MODE_STATIC,
    "MOBILE": IAQMode.IAQ_MODE_MOBILE,
}

SupplyVoltage = bme680_bsec_ns.enum("SupplyVoltage")
SUPPLY_VOLTAGE_OPTIONS = {
    "1.8V": SupplyVoltage.SUPPLY_VOLTAGE_1V8,
    "3.3V": SupplyVoltage.SUPPLY_VOLTAGE_3V3,
}

SampleRate = bme680_bsec_ns.enum("SampleRate")
SAMPLE_RATE_OPTIONS = {
    "LP": SampleRate.SAMPLE_RATE_LP,
    "ULP": SampleRate.SAMPLE_RATE_ULP,
}

BME680BSECComponent = bme680_bsec_ns.class_(
    "BME680BSECComponent", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BME680BSECComponent),
            cv.Optional(CONF_TEMPERATURE_OFFSET, default=0): cv.temperature,
            cv.Optional(CONF_IAQ_MODE, default="STATIC"): cv.enum(
                IAQ_MODE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_SUPPLY_VOLTAGE, default="3.3V"): cv.enum(
                SUPPLY_VOLTAGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_SAMPLE_RATE, default="LP"): cv.enum(
                SAMPLE_RATE_OPTIONS, upper=True
            ),
            cv.Optional(
                CONF_STATE_SAVE_INTERVAL, default="6hours"
            ): cv.positive_time_period_minutes,
        }
    ).extend(i2c.i2c_device_schema(0x76)),
    cv.only_with_arduino,
    cv.Any(
        cv.only_on_esp8266,
        cv.All(
            cv.only_on_esp32,
            esp32.only_on_variant(supported=[esp32.const.VARIANT_ESP32]),
        ),
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_device_id(str(config[CONF_ID])))
    cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))
    cg.add(var.set_iaq_mode(config[CONF_IAQ_MODE]))
    cg.add(var.set_supply_voltage(config[CONF_SUPPLY_VOLTAGE]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(
        var.set_state_save_interval(config[CONF_STATE_SAVE_INTERVAL].total_milliseconds)
    )

    # Although this component does not use SPI, the BSEC library requires the SPI library
    cg.add_library("SPI", None)

    cg.add_define("USE_BSEC")
    cg.add_library("boschsensortec/BSEC Software Library", "1.6.1480")
