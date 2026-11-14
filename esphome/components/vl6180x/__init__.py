"""Configuration schema and code generation for the VL6180X / VL6180 sensor."""

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.i2c import CONF_I2C_ID
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ENABLE_PIN, CONF_ID, CONF_INTERRUPT_PIN
from esphome.types import ConfigType

AUTO_LOAD = ["sensor", "text_sensor"]
DEPENDENCIES = ["i2c"]
DOMAIN = "vl6180x"
CODEOWNERS = ["@NonaSuomy"]
MULTI_CONF = True

vl6180x_ns = cg.esphome_ns.namespace("vl6180x")
VL6180XComponent = vl6180x_ns.class_(
    "VL6180XComponent", cg.PollingComponent, i2c.I2CDevice
)
_request_instance_slot = cg.slot_counter("VL6180X_INSTANCE_COUNT")

CONF_SCALER = "scaler"
CONF_VL6180X_ID = "vl6180x_id"
CONF_RECOVERY = "recovery"
CONF_FAILURE_THRESHOLD = "failure_threshold"
CONF_RETRY_INTERVAL = "retry_interval"
CONF_MAX_RETRIES = "max_retries"

ALS_GAINS = {
    1: 6,
    1.25: 5,
    1.67: 4,
    2.5: 3,
    5: 2,
    10: 1,
    20: 0,
    40: 7,
    "1x": 6,
    "1.25x": 5,
    "1.67x": 4,
    "2.5x": 3,
    "5x": 2,
    "10x": 1,
    "20x": 0,
    "40x": 7,
}
ALS_GAIN_FACTORS = {6: 1.0, 5: 1.25, 4: 1.67, 3: 2.5, 2: 5.0, 1: 10.0, 0: 20.0, 7: 40.0}


RECOVERY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FAILURE_THRESHOLD, default=3): cv.int_range(min=1, max=255),
        cv.Optional(
            CONF_RETRY_INTERVAL, default="30s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_RETRIES, default=10): cv.int_range(min=1, max=255),
    }
)

_ENTRY_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VL6180XComponent),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RECOVERY): RECOVERY_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29))
)


CONFIG_SCHEMA = cv.ensure_list(_ENTRY_SCHEMA)


def _final_validate(configs: list[ConfigType]) -> list[ConfigType]:
    for index, config in enumerate(configs):
        siblings = [
            entry for entry in configs if entry[CONF_I2C_ID] == config[CONF_I2C_ID]
        ]
        if config[CONF_ADDRESS] != 0x29 and CONF_ENABLE_PIN not in config:
            raise cv.Invalid(
                "enable_pin is required when changing the sensor's default I2C address",
                path=[index, CONF_ENABLE_PIN],
            )
        if len(siblings) <= 1:
            continue
        if CONF_ENABLE_PIN not in config:
            raise cv.Invalid(
                "Every VL6180X on a shared I2C bus needs an enable_pin",
                path=[index, CONF_ENABLE_PIN],
            )
        pin_config = config[CONF_ENABLE_PIN]
        pin_number = pin_config["number"]
        pin_provider = repr(
            {
                key: value
                for key, value in pin_config.items()
                if key not in ("number", "mode", "inverted")
            }
        )
        for sibling_index, sibling in enumerate(configs):
            if sibling_index == index or sibling[CONF_I2C_ID] != config[CONF_I2C_ID]:
                continue
            sibling_pin = sibling.get(CONF_ENABLE_PIN)
            if sibling_pin is not None:
                sibling_provider = repr(
                    {
                        key: value
                        for key, value in sibling_pin.items()
                        if key not in ("number", "mode", "inverted")
                    }
                )
                if (
                    pin_provider == sibling_provider
                    and pin_number == sibling_pin["number"]
                ):
                    raise cv.Invalid(
                        "Each VL6180X on a shared I2C bus needs a unique enable_pin",
                        path=[index, CONF_ENABLE_PIN],
                    )
            if sibling[CONF_ADDRESS] == config[CONF_ADDRESS]:
                raise cv.Invalid(
                    f"Address 0x{config[CONF_ADDRESS]:02X} is used more than once on this I2C bus",
                    path=[index, CONF_ADDRESS],
                )
    return configs


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: list[ConfigType]) -> None:
    for entry in config:
        var = cg.new_Pvariable(entry[CONF_ID])
        await cg.register_component(var, entry)
        await i2c.register_i2c_device(var, entry)

        cg.add(var.set_name_str(entry[CONF_ID].id))

        _request_instance_slot()
        cg.add(var.register_instance(var))

        if CONF_ENABLE_PIN in entry:
            pin = await cg.gpio_pin_expression(entry[CONF_ENABLE_PIN])
            cg.add(var.set_enable_pin(pin))

        if CONF_INTERRUPT_PIN in entry:
            pin = await cg.gpio_pin_expression(entry[CONF_INTERRUPT_PIN])
            cg.add(var.set_interrupt_pin(pin))

        if CONF_RECOVERY in entry:
            rec = entry[CONF_RECOVERY]
            cg.add(var.set_failure_threshold(rec[CONF_FAILURE_THRESHOLD]))
            cg.add(var.set_retry_interval(rec[CONF_RETRY_INTERVAL]))
            cg.add(var.set_max_retries(rec[CONF_MAX_RETRIES]))
