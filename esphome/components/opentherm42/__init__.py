from esphome import pins
import esphome.codegen as cg
from esphome.components import time as time_
from esphome.components.esp32 import include_builtin_idf_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME_ID, PLATFORM_ESP32, PLATFORM_ESP8266
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import CONF_OPENTHERM42_ID

CODEOWNERS = ["@fornellas"]
MULTI_CONF = True
# hub.h unconditionally declares fields of each of these types (e.g. every possible boiler sensor,
# even ones the user hasn't configured), so their headers must always be compiled in -- regardless
# of whether the user's YAML happens to configure any entities under these domains.
AUTO_LOAD = [
    "binary_sensor",
    "number",
    "select",
    "sensor",
    "switch",
    "text_sensor",
    "time",
]

CONF_IN_PIN = "in_pin"
CONF_OUT_PIN = "out_pin"

# §5.3.2 Class 2: this master's own identity, written to the boiler once at startup (IDs 2 LB/124/126).
# Kept as static hub-level config rather than entities -- unlike the boiler's status/measurements,
# this doesn't change at runtime, so there's nothing for Home Assistant to show or control.
CONF_CONTROLLER_MEMBER_ID_CODE = "controller_member_id_code"
CONF_CONTROLLER_OPENTHERM_VERSION = "controller_opentherm_version"
CONF_CONTROLLER_PRODUCT_TYPE = "controller_product_type"
CONF_CONTROLLER_PRODUCT_VERSION = "controller_product_version"

opentherm42_ns = cg.esphome_ns.namespace("opentherm42")
OpenTherm42Hub = opentherm42_ns.class_("OpenTherm42Hub", cg.Component)


def validate_requires_time_id(marker: str):
    """FINAL_VALIDATE_SCHEMA factory for a platform entity that only makes sense when the opentherm42
    hub it references has time_id set (e.g. the time-sync status binary_sensor and button): fails
    config validation, rather than silently doing nothing at runtime, if marker is configured but the
    hub's time_id is not.
    """

    def _validate(config: ConfigType) -> None:
        if marker not in config:
            return
        full_config = fv.full_config.get()
        hub_path = full_config.get_path_for_id(config[CONF_OPENTHERM42_ID])[:-1]
        hub_config = full_config.get_config_for_path(hub_path)
        if CONF_TIME_ID not in hub_config:
            raise cv.Invalid(
                f"'{marker}' requires the opentherm42 hub to have 'time_id' set",
                path=[marker],
            )

    return _validate


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenTherm42Hub),
            cv.Required(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
            # §5.2.1 Note 2: a MemberID code of 0 signifies a customer non-specific device.
            cv.Optional(CONF_CONTROLLER_MEMBER_ID_CODE, default=0): cv.int_range(
                min=0, max=255
            ),
            # Defaults to the protocol version this component implements.
            cv.Optional(CONF_CONTROLLER_OPENTHERM_VERSION, default=4.2): cv.float_range(
                min=0, max=127
            ),
            cv.Optional(CONF_CONTROLLER_PRODUCT_TYPE, default=0): cv.int_range(
                min=0, max=255
            ),
            cv.Optional(CONF_CONTROLLER_PRODUCT_VERSION, default=0): cv.int_range(
                min=0, max=255
            ),
            # §5.3.4 Class 4, IDs 20/21/22: if set, this master keeps the boiler's Day-of-week/Time,
            # Date and Year synced to this clock. Left unset, those three ids are never sent.
            cv.Optional(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    # datalink.cpp only implements a hardware-timer backend for ESP32 (gptimer) and ESP8266 (Arduino
    # Timer1) -- RP2040 and LibreTiny have no backend, so this matches the same restriction the
    # earlier opentherm component uses.
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266]),
)


async def to_code(config: dict) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    in_pin = await cg.gpio_pin_expression(config[CONF_IN_PIN])
    cg.add(var.set_in_pin(in_pin))
    out_pin = await cg.gpio_pin_expression(config[CONF_OUT_PIN])
    cg.add(var.set_out_pin(out_pin))

    cg.add(var.set_controller_member_id_code(config[CONF_CONTROLLER_MEMBER_ID_CODE]))
    cg.add(
        var.set_controller_opentherm_version(config[CONF_CONTROLLER_OPENTHERM_VERSION])
    )
    cg.add(var.set_controller_product_type(config[CONF_CONTROLLER_PRODUCT_TYPE]))
    cg.add(var.set_controller_product_version(config[CONF_CONTROLLER_PRODUCT_VERSION]))

    if (time_id := config.get(CONF_TIME_ID)) is not None:
        time_var = await cg.get_variable(time_id)
        cg.add(var.set_time_id(time_var))

    if CORE.is_esp32:
        # §4.3/§3.3.2 bit-timing (datalink.h) needs a hardware timer for microsecond-accurate sampling.
        include_builtin_idf_component("esp_driver_gptimer")
