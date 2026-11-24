import logging
from typing import Any

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt, sensor
from esphome.components.const import CONF_OUT_PIN
from esphome.components.esp32 import include_builtin_idf_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RMT_SYMBOLS, PLATFORM_ESP32, PLATFORM_ESP8266
from esphome.core import CORE

from . import const, generate, schema, validate

CODEOWNERS = ["@olegtarasov"]
_LOGGER = logging.getLogger(__name__)
DOMAIN = "opentherm"
MULTI_CONF = True
AUTO_LOAD = ["remote_base"]

CONF_IN_PIN = "in_pin"
CONF_CH_ENABLE = "ch_enable"
CONF_DHW_ENABLE = "dhw_enable"
CONF_COOLING_ENABLE = "cooling_enable"
CONF_OTC_ACTIVE = "otc_active"
CONF_CH2_ACTIVE = "ch2_active"
CONF_SUMMER_MODE_ACTIVE = "summer_mode_active"
CONF_DHW_BLOCK = "dhw_block"
CONF_SYNC_MODE = "sync_mode"
CONF_BEFORE_SEND = "before_send"
CONF_BEFORE_PROCESS_RESPONSE = "before_process_response"

_DATA_ARGS = [(generate.OpenthermData.operator("ref"), "x")]
_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_BEFORE_SEND, "add_on_before_send_callback", _DATA_ARGS
    ),
    automation.CallbackAutomation(
        CONF_BEFORE_PROCESS_RESPONSE,
        "add_on_before_process_response_callback",
        _DATA_ARGS,
    ),
)


def _set_rmt_symbols(config: dict[str, Any]) -> dict[str, Any]:
    """Pin rmt_symbols to one RMT memory block.

    A frame is 34 bits, so at most 68 edges, and the smallest channel the driver accepts is one memory
    block. The value is injected rather than offered as an option, so esp32_rmt.request_rmt_channel()
    can account for the channel without the key being user-settable.
    """
    if CORE.is_esp32:
        memory = esp32_rmt.RMT_MEMORY.get(esp32.get_esp32_variant())
        if memory is not None:
            config[CONF_RMT_SYMBOLS] = memory.symbols_per_block
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(generate.OpenTherm),
            cv.Required(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
            cv.Required(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_CH_ENABLE, True): cv.boolean,
            cv.Optional(CONF_DHW_ENABLE, True): cv.boolean,
            cv.Optional(CONF_COOLING_ENABLE, False): cv.boolean,
            cv.Optional(CONF_OTC_ACTIVE, False): cv.boolean,
            cv.Optional(CONF_CH2_ACTIVE, False): cv.boolean,
            cv.Optional(CONF_SUMMER_MODE_ACTIVE, False): cv.boolean,
            cv.Optional(CONF_DHW_BLOCK, False): cv.boolean,
            cv.Optional(CONF_SYNC_MODE, False): cv.boolean,
            cv.Optional(CONF_BEFORE_SEND): automation.validate_automation({}),
            cv.Optional(CONF_BEFORE_PROCESS_RESPONSE): automation.validate_automation(
                {}
            ),
        }
    )
    .extend(
        validate.create_entities_schema(
            schema.INPUTS, (lambda _: cv.use_id(sensor.Sensor))
        )
    )
    .extend(
        validate.create_entities_schema(
            schema.SETTINGS, (lambda s: s.validation_schema)
        )
    )
    .extend(cv.COMPONENT_SCHEMA)
    .add_extra(_set_rmt_symbols)
    # setup_priority::HARDWARE (800), which OpenTherm::get_setup_priority() returns. The transport
    # creates one RX and one TX channel, so both are registered for the shared memory accounting.
    .add_extra(esp32_rmt.request_rmt_channel("RX", 800.0))
    .add_extra(esp32_rmt.request_rmt_channel("TX", 800.0)),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266]),
)


def _rmt_transport() -> bool:
    """True when this build compiles the RMT transport, mirroring USE_ESP32 && SOC_RMT_SUPPORTED."""
    return CORE.is_esp32 and esp32.get_esp32_variant() not in esp32_rmt.VARIANTS_NO_RMT


def _final_validate(config: dict[str, Any]) -> None:
    # Shared channel accounting for every RMT consumer in the config.
    esp32_rmt.final_validate_rmt_channels(config)
    if config.get(CONF_SYNC_MODE) and _rmt_transport():
        _LOGGER.warning(
            "'%s' has no effect as this target uses the RMT peripheral, and can be safely removed from the %s component.",
            CONF_SYNC_MODE,
            config[CONF_ID],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: dict[str, Any]) -> None:
    if CORE.is_esp8266:
        from esphome.components.esp8266.const import use_timer1

        # Per hub: every hub programs the timer and the interrupt has one instance pointer
        use_timer1(f"{DOMAIN} {config[CONF_ID]}")
    rmt = _rmt_transport()
    if CORE.is_esp32:
        include_builtin_idf_component("esp_driver_gptimer")
        if rmt:
            include_builtin_idf_component("esp_driver_rmt")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    esp32_rmt.register_rmt_channel(config)

    # Set pins
    await cg.add_pin(var.set_in_pin, config[CONF_IN_PIN])
    await cg.add_pin(var.set_out_pin, config[CONF_OUT_PIN])

    if rmt:
        cg.add(var.set_rmt_symbols(config[CONF_RMT_SYMBOLS]))
        # One uint32_t word per RMT symbol
        cg.add(
            var.set_buffer(
                cg.static_buffer(config[CONF_ID], cg.uint32, config[CONF_RMT_SYMBOLS])
            )
        )

    # sync_mode is decided here rather than in the loop below: the flag only exists on the timer transport.
    if not rmt:
        cg.add(var.set_sync_mode(config[CONF_SYNC_MODE]))

    non_sensors = {
        CONF_ID,
        CONF_IN_PIN,
        CONF_OUT_PIN,
        CONF_SYNC_MODE,
        CONF_BEFORE_SEND,
        CONF_BEFORE_PROCESS_RESPONSE,
        CONF_RMT_SYMBOLS,
    }
    input_sensors = []
    settings = []
    for key, value in config.items():
        if key in non_sensors:
            continue
        if key in schema.INPUTS:
            await cg.add_variable(
                getattr(var, f"set_{key}_{const.INPUT_SENSOR}"), value
            )
            input_sensors.append(key)
        elif key in schema.SETTINGS:
            if value == schema.SETTINGS[key].default_value:
                continue
            cg.add(getattr(var, f"set_{key}_{const.SETTING}")(value))
            settings.append(key)
        else:
            cg.add(getattr(var, f"set_{key}")(value))

    if len(input_sensors) > 0:
        generate.define_has_component(const.INPUT_SENSOR, input_sensors)
        generate.define_message_handler(
            const.INPUT_SENSOR, input_sensors, schema.INPUTS
        )
        generate.define_readers(const.INPUT_SENSOR, input_sensors)
        generate.add_messages(var, input_sensors, schema.INPUTS)

    if len(settings) > 0:
        generate.define_has_settings(settings, schema.SETTINGS)
        generate.define_message_handler(const.SETTING, settings, schema.SETTINGS)
        generate.define_setting_readers(const.SETTING, settings)
        generate.add_messages(var, settings, schema.SETTINGS)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
