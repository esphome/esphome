import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_CUSTOM_FAN_MODES, CONF_CUSTOM_PRESETS, CONF_ID

from .. import (
    CONF_ECONET_ID,
    CONF_REQUEST_MOD,
    CONF_REQUEST_ONCE,
    CONF_SRC_ADDRESS,
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    econet_ns,
)

DEPENDENCIES = ["econet"]

CONF_CURRENT_TEMPERATURE_DATAPOINT = "current_temperature_datapoint"
CONF_TARGET_TEMPERATURE_DATAPOINT = "target_temperature_datapoint"
CONF_TARGET_TEMPERATURE_LOW_DATAPOINT = "target_temperature_low_datapoint"
CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT = "target_temperature_high_datapoint"
CONF_MODE_DATAPOINT = "mode_datapoint"
CONF_CUSTOM_PRESET_DATAPOINT = "custom_preset_datapoint"
CONF_CUSTOM_FAN_MODE_DATAPOINT = "custom_fan_mode_datapoint"
CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT = "custom_fan_mode_no_schedule_datapoint"
CONF_FOLLOW_SCHEDULE_DATAPOINT = "follow_schedule_datapoint"
CONF_MODES = "modes"
CONF_CURRENT_HUMIDITY_DATAPOINT = "current_humidity_datapoint"
CONF_TARGET_DEHUMIDIFICATION_LEVEL_DATAPOINT = "target_dehumidification_level_datapoint"

EconetClimate = econet_ns.class_(
    "EconetClimate", climate.Climate, cg.Component, EconetClient
)


def ensure_climate_mode_map(value):
    cv.check_not_templatable(value)
    options_map_schema = cv.Schema({cv.uint8_t: climate.validate_climate_mode})
    value = options_map_schema(value)
    all_values = list(value.keys())
    unique_values = set(value.keys())
    if len(all_values) != len(unique_values):
        raise cv.Invalid("Mapping values must be unique.")
    return value


def ensure_option_map(value):
    cv.check_not_templatable(value)
    options_map_schema = cv.Schema({cv.uint8_t: cv.string_strict})
    value = options_map_schema(value)
    all_values = list(value.keys())
    unique_values = set(value.keys())
    if len(all_values) != len(unique_values):
        raise cv.Invalid("Mapping values must be unique.")
    return value


CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EconetClimate),
            cv.Optional(CONF_CURRENT_TEMPERATURE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_TARGET_TEMPERATURE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_TARGET_TEMPERATURE_LOW_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_MODE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_CUSTOM_PRESET_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_CUSTOM_FAN_MODE_DATAPOINT, default=""): cv.string,
            cv.Optional(
                CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT, default=""
            ): cv.string,
            cv.Optional(CONF_FOLLOW_SCHEDULE_DATAPOINT, default=""): cv.string,
            cv.Optional(CONF_MODES, default={}): ensure_climate_mode_map,
            cv.Optional(CONF_CUSTOM_PRESETS, default={}): ensure_option_map,
            cv.Optional(CONF_CUSTOM_FAN_MODES, default={}): ensure_option_map,
            cv.Optional(CONF_CURRENT_HUMIDITY_DATAPOINT, default=""): cv.string,
            cv.Optional(
                CONF_TARGET_DEHUMIDIFICATION_LEVEL_DATAPOINT, default=""
            ): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECONET_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    paren = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet_parent(paren))
    cg.add(var.set_request_mod(config[CONF_REQUEST_MOD]))
    cg.add(var.set_request_once(config[CONF_REQUEST_ONCE]))
    cg.add(var.set_src_adr(config[CONF_SRC_ADDRESS]))
    cg.add(var.set_current_temperature_id(config[CONF_CURRENT_TEMPERATURE_DATAPOINT]))
    cg.add(var.set_target_temperature_id(config[CONF_TARGET_TEMPERATURE_DATAPOINT]))
    cg.add(
        var.set_target_temperature_low_id(config[CONF_TARGET_TEMPERATURE_LOW_DATAPOINT])
    )
    cg.add(
        var.set_target_temperature_high_id(
            config[CONF_TARGET_TEMPERATURE_HIGH_DATAPOINT]
        )
    )
    cg.add(var.set_mode_id(config[CONF_MODE_DATAPOINT]))
    cg.add(var.set_custom_preset_id(config[CONF_CUSTOM_PRESET_DATAPOINT]))
    cg.add(var.set_custom_fan_mode_id(config[CONF_CUSTOM_FAN_MODE_DATAPOINT]))
    cg.add(
        var.set_custom_fan_mode_no_schedule_id(
            config[CONF_CUSTOM_FAN_MODE_NO_SCHEDULE_DATAPOINT]
        )
    )
    cg.add(var.set_follow_schedule_id(config[CONF_FOLLOW_SCHEDULE_DATAPOINT]))
    cg.add(
        var.set_modes(
            list(config[CONF_MODES].keys()),
            list(config[CONF_MODES].values()),
        )
    )
    cg.add(
        var.set_custom_presets(
            list(config[CONF_CUSTOM_PRESETS].keys()),
            list(config[CONF_CUSTOM_PRESETS].values()),
        )
    )
    cg.add(
        var.set_custom_fan_modes(
            list(config[CONF_CUSTOM_FAN_MODES].keys()),
            list(config[CONF_CUSTOM_FAN_MODES].values()),
        )
    )
    cg.add(var.set_current_humidity_id(config[CONF_CURRENT_HUMIDITY_DATAPOINT]))
    cg.add(
        var.set_target_dehumidification_level_id(
            config[CONF_TARGET_DEHUMIDIFICATION_LEVEL_DATAPOINT]
        )
    )
