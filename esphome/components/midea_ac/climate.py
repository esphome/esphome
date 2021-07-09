from esphome.components import (
    climate,
    sensor,
    remote_base,
    remote_transmitter,
)
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.const import (
    CONF_CUSTOM_FAN_MODES,
    CONF_CUSTOM_PRESETS,
    CONF_ID,
    CONF_PRESET_BOOST,
    CONF_PRESET_ECO,
    CONF_PRESET_SLEEP,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_WATT,
    ICON_THERMOMETER,
    ICON_POWER,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ICON_WATER_PERCENT,
    DEVICE_CLASS_HUMIDITY,
    CONF_TEMPERATURE,
)
from esphome.components.midea_dongle import CONF_MIDEA_DONGLE_ID, MideaDongle

AUTO_LOAD = ["climate", "sensor", "midea_dongle", "remote_base"]
CODEOWNERS = ["@dudanov"]
CONF_BEEPER = "beeper"
CONF_SWING_HORIZONTAL = "swing_horizontal"
CONF_SWING_BOTH = "swing_both"
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_POWER_USAGE = "power_usage"
CONF_HUMIDITY_SETPOINT = "humidity_setpoint"
midea_ac_ns = cg.esphome_ns.namespace("midea_ac")
MideaAC = midea_ac_ns.class_("MideaAC", climate.Climate, cg.Component)
# Actions
FollowMeAction = midea_ac_ns.class_(
    "FollowMeAction", remote_base.RemoteTransmitterActionBase
)
DisplayToggleAction = midea_ac_ns.class_(
    "MideaDisplayToggleAction", remote_base.RemoteTransmitterActionBase
)
SwingStepAction = midea_ac_ns.class_(
    "MideaSwingStepAction", remote_base.RemoteTransmitterActionBase
)

# Midea FollowMe action
CONF_BEEPER = "beeper"
MIDEA_FOLLOW_ME_MIN = 0
MIDEA_FOLLOW_ME_MAX = 37
MIDEA_FOLLOW_ME_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEMPERATURE): cv.templatable(
            cv.int_range(MIDEA_FOLLOW_ME_MIN, MIDEA_FOLLOW_ME_MAX),
        ),
        cv.Optional(CONF_BEEPER, default=False): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "midea_ac.follow_me", FollowMeAction, MIDEA_FOLLOW_ME_SCHEMA
)
async def follow_me_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_BEEPER], args, cg.uint8)
    cg.add(var.set_beeper(template_))
    template_ = await cg.templatable(config[CONF_TEMPERATURE], args, cg.uint8)
    cg.add(var.set_temperature(template_))
    return var


# Midea ToggleLight action
@automation.register_action(
    "midea_ac.display_toggle",
    DisplayToggleAction,
    cv.Schema({}),
)
async def midea_display_toggle_action(var, config, args):
    pass


# Midea Swing Step action
@automation.register_action(
    "midea_ac.swing_step",
    SwingStepAction,
    cv.Schema({}),
)
async def midea_swing_step_action(var, config, args):
    pass


CLIMATE_CUSTOM_FAN_MODES = {
    "SILENT": "silent",
    "TURBO": "turbo",
}

validate_climate_custom_fan_mode = cv.enum(CLIMATE_CUSTOM_FAN_MODES, upper=True)

CLIMATE_CUSTOM_PRESETS = {
    "FREEZE_PROTECTION": "freeze protection",
}

validate_climate_custom_preset = cv.enum(CLIMATE_CUSTOM_PRESETS, upper=True)

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MideaAC),
            cv.GenerateID(CONF_MIDEA_DONGLE_ID): cv.use_id(MideaDongle),
            cv.Optional(CONF_TRANSMITTER_ID): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
            cv.Optional(CONF_BEEPER, default=False): cv.boolean,
            cv.Optional(CONF_CUSTOM_FAN_MODES): cv.ensure_list(
                validate_climate_custom_fan_mode
            ),
            cv.Optional(CONF_CUSTOM_PRESETS): cv.ensure_list(
                validate_climate_custom_preset
            ),
            cv.Optional(CONF_SWING_HORIZONTAL, default=False): cv.boolean,
            cv.Optional(CONF_SWING_BOTH, default=False): cv.boolean,
            cv.Optional(CONF_PRESET_ECO, default=False): cv.boolean,
            cv.Optional(CONF_PRESET_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_PRESET_BOOST, default=False): cv.boolean,
            cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS,
                ICON_THERMOMETER,
                0,
                DEVICE_CLASS_TEMPERATURE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_USAGE): sensor.sensor_schema(
                UNIT_WATT, ICON_POWER, 0, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
            ),
            cv.Optional(CONF_HUMIDITY_SETPOINT): sensor.sensor_schema(
                UNIT_PERCENT,
                ICON_WATER_PERCENT,
                0,
                DEVICE_CLASS_HUMIDITY,
                STATE_CLASS_MEASUREMENT,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    paren = await cg.get_variable(config[CONF_MIDEA_DONGLE_ID])
    cg.add(var.set_midea_dongle_parent(paren))
    cg.add(var.set_beeper_feedback(config[CONF_BEEPER]))
    cg.add(var.set_swing_horizontal(config[CONF_SWING_HORIZONTAL]))
    cg.add(var.set_swing_both(config[CONF_SWING_BOTH]))
    cg.add(var.set_preset_eco(config[CONF_PRESET_ECO]))
    cg.add(var.set_preset_sleep(config[CONF_PRESET_SLEEP]))
    cg.add(var.set_preset_boost(config[CONF_PRESET_BOOST]))
    if CONF_TRANSMITTER_ID in config:
        transmitter_ = await cg.get_variable(config[CONF_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter_))
    if CONF_CUSTOM_FAN_MODES in config:
        cg.add(var.set_custom_fan_modes(config[CONF_CUSTOM_FAN_MODES]))
    if CONF_CUSTOM_PRESETS in config:
        cg.add(var.set_custom_presets(config[CONF_CUSTOM_PRESETS]))
    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature_sensor(sens))
    if CONF_POWER_USAGE in config:
        sens = await sensor.new_sensor(config[CONF_POWER_USAGE])
        cg.add(var.set_power_sensor(sens))
    if CONF_HUMIDITY_SETPOINT in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY_SETPOINT])
        cg.add(var.set_humidity_setpoint_sensor(sens))
