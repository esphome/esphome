from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.esp32 import include_builtin_idf_component
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANGE_MODE_EVERY,
    CONF_CURRENT,
    CONF_CURRENT_RESISTOR,
    CONF_ENERGY,
    CONF_ID,
    CONF_INITIAL_MODE,
    CONF_MODEL,
    CONF_POWER,
    CONF_SEL_PIN,
    CONF_VOLTAGE,
    CONF_VOLTAGE_DIVIDER,
)
from esphome.core import CORE
from esphome.types import ConfigType

AUTO_LOAD = ["pulse_counter"]

hlw8012_ns = cg.esphome_ns.namespace("hlw8012")
HLW8012Component = hlw8012_ns.class_("HLW8012Component", cg.PollingComponent)
HLW8012InitialMode = hlw8012_ns.enum("HLW8012InitialMode")
HLW8012SensorModels = hlw8012_ns.enum("HLW8012SensorModels")

INITIAL_MODES = {
    CONF_CURRENT: HLW8012InitialMode.HLW8012_INITIAL_MODE_CURRENT,
    CONF_VOLTAGE: HLW8012InitialMode.HLW8012_INITIAL_MODE_VOLTAGE,
}

MODELS = HLW8012SensorModels.options(
    "HLW8012", "CSE7759", "BL0937", prefix="HLW8012_SENSOR_MODEL_"
)

CONF_CF1_PIN = "cf1_pin"
CONF_CF_PIN = "cf_pin"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HLW8012Component),
        cv.Required(CONF_SEL_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CF_PIN): cv.All(pins.internal_gpio_input_pullup_pin_schema),
        cv.Required(CONF_CF1_PIN): cv.All(pins.internal_gpio_input_pullup_pin_schema),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            kind=sensor.VOLTAGE, accuracy_decimals=1
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            kind=sensor.CURRENT, accuracy_decimals=2
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            kind=sensor.POWER, accuracy_decimals=1
        ),
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            kind=sensor.ENERGY_WH, accuracy_decimals=1
        ),
        cv.Optional(CONF_CURRENT_RESISTOR, default=0.001): cv.resistance,
        cv.Optional(CONF_VOLTAGE_DIVIDER, default=2351): cv.positive_float,
        cv.Optional(CONF_MODEL, default="HLW8012"): cv.enum(MODELS, upper=True),
        cv.Optional(CONF_CHANGE_MODE_EVERY, default=8): cv.Any(
            "never",
            cv.All(cv.uint32_t, cv.Range(min=1)),
        ),
        cv.Optional(CONF_INITIAL_MODE, default=CONF_VOLTAGE): cv.one_of(
            *INITIAL_MODES, lower=True
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config: ConfigType) -> None:
    if CORE.is_esp32:
        include_builtin_idf_component("esp_driver_pcnt")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.add_pin(var.set_sel_pin, config[CONF_SEL_PIN])
    await cg.add_pin(var.set_cf_pin, config[CONF_CF_PIN])
    await cg.add_pin(var.set_cf1_pin, config[CONF_CF1_PIN])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
    await sensors(CONF_ENERGY, var.set_energy_sensor)
    cg.add(var.set_current_resistor(config[CONF_CURRENT_RESISTOR]))
    cg.add(var.set_voltage_divider(config[CONF_VOLTAGE_DIVIDER]))
    cg.add(var.set_initial_mode(INITIAL_MODES[config[CONF_INITIAL_MODE]]))
    cg.add(var.set_sensor_model(config[CONF_MODEL]))

    interval = config[CONF_CHANGE_MODE_EVERY]
    if interval == "never":
        interval = 0
    cg.add(var.set_change_mode_every(interval))
