import logging
from typing import Any

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID, PLATFORM_ESP32, PLATFORM_ESP8266

from . import const, generate, schema, validate

CODEOWNERS = ["@olegtarasov"]
MULTI_CONF = True

CONF_IN_PIN = "in_pin"
CONF_OUT_PIN = "out_pin"
CONF_CH_ENABLE = "ch_enable"
CONF_DHW_ENABLE = "dhw_enable"
CONF_COOLING_ENABLE = "cooling_enable"
CONF_OTC_ACTIVE = "otc_active"
CONF_CH2_ACTIVE = "ch2_active"
CONF_SUMMER_MODE_ACTIVE = "summer_mode_active"
CONF_DHW_BLOCK = "dhw_block"
CONF_SYNC_MODE = "sync_mode"
CONF_OPENTHERM_VERSION = "opentherm_version"  # Deprecated, will be removed
CONF_BEFORE_SEND = "before_send"
CONF_BEFORE_PROCESS_RESPONSE = "before_process_response"

# Triggers
BeforeSendTrigger = generate.opentherm_ns.class_(
    "BeforeSendTrigger",
    automation.Trigger.template(generate.OpenthermData.operator("ref")),
)
BeforeProcessResponseTrigger = generate.opentherm_ns.class_(
    "BeforeProcessResponseTrigger",
    automation.Trigger.template(generate.OpenthermData.operator("ref")),
)

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(generate.OpenthermHub),
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
            cv.Optional(CONF_OPENTHERM_VERSION): cv.positive_float,  # Deprecated
            cv.Optional(CONF_BEFORE_SEND): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BeforeSendTrigger),
                }
            ),
            cv.Optional(CONF_BEFORE_PROCESS_RESPONSE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BeforeProcessResponseTrigger
                    ),
                }
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
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266]),
)


async def to_code(config: dict[str, Any]) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set pins
    in_pin = await cg.gpio_pin_expression(config[CONF_IN_PIN])
    cg.add(var.set_in_pin(in_pin))

    out_pin = await cg.gpio_pin_expression(config[CONF_OUT_PIN])
    cg.add(var.set_out_pin(out_pin))

    non_sensors = {
        CONF_ID,
        CONF_IN_PIN,
        CONF_OUT_PIN,
        CONF_BEFORE_SEND,
        CONF_BEFORE_PROCESS_RESPONSE,
    }
    input_sensors = []
    settings = []
    for key, value in config.items():
        if key in non_sensors:
            continue
        if key in schema.INPUTS:
            input_sensor = await cg.get_variable(value)
            cg.add(getattr(var, f"set_{key}_{const.INPUT_SENSOR}")(input_sensor))
            input_sensors.append(key)
        elif key in schema.SETTINGS:
            if value == schema.SETTINGS[key].default_value:
                continue
            cg.add(getattr(var, f"set_{key}_{const.SETTING}")(value))
            settings.append(key)
        else:
            if key == CONF_OPENTHERM_VERSION:
                _LOGGER.warning(
                    "opentherm_version is deprecated and will be removed in esphome 2025.2.0\n"
                    "Please change to 'opentherm_version_controller'."
                )
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

    for conf in config.get(CONF_BEFORE_SEND, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(generate.OpenthermData.operator("ref"), "x")], conf
        )

    for conf in config.get(CONF_BEFORE_PROCESS_RESPONSE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(generate.OpenthermData.operator("ref"), "x")], conf
        )
