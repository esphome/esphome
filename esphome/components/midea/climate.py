from esphome.core import coroutine
from esphome import automation
from esphome.components import climate, sensor, uart, remote_transmitter
from esphome.components.remote_base import CONF_TRANSMITTER_ID
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_AUTOCONF,
    CONF_BEEPER,
    CONF_CUSTOM_FAN_MODES,
    CONF_CUSTOM_PRESETS,
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_PERIOD,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_PRESETS,
    CONF_SUPPORTED_SWING_MODES,
    CONF_TIMEOUT,
    CONF_TEMPERATURE,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    ICON_POWER,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_WATT,
)
from esphome.components.climate import (
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
)

CODEOWNERS = ["@dudanov"]
DEPENDENCIES = ["climate", "uart"]
AUTO_LOAD = ["sensor"]
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_POWER_USAGE = "power_usage"
CONF_HUMIDITY_SETPOINT = "humidity_setpoint"
midea_ac_ns = cg.esphome_ns.namespace("midea").namespace("ac")
AirConditioner = midea_ac_ns.class_("AirConditioner", climate.Climate, cg.Component)
Capabilities = midea_ac_ns.namespace("Constants")


def templatize(value):
    if isinstance(value, cv.Schema):
        value = value.schema
    ret = {}
    for key, val in value.items():
        ret[key] = cv.templatable(val)
    return cv.Schema(ret)


def register_action(name, type_, schema):
    validator = templatize(schema).extend(MIDEA_ACTION_BASE_SCHEMA)
    registerer = automation.register_action(f"midea_ac.{name}", type_, validator)

    def decorator(func):
        async def new_func(config, action_id, template_arg, args):
            ac_ = await cg.get_variable(config[CONF_ID])
            var = cg.new_Pvariable(action_id, template_arg)
            cg.add(var.set_parent(ac_))
            await coroutine(func)(var, config, args)
            return var

        return registerer(new_func)

    return decorator


ALLOWED_CLIMATE_MODES = {
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
}

ALLOWED_CLIMATE_PRESETS = {
    "ECO": ClimatePreset.CLIMATE_PRESET_ECO,
    "BOOST": ClimatePreset.CLIMATE_PRESET_BOOST,
    "SLEEP": ClimatePreset.CLIMATE_PRESET_SLEEP,
}

ALLOWED_CLIMATE_SWING_MODES = {
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
}

CUSTOM_FAN_MODES = {
    "SILENT": Capabilities.SILENT,
    "TURBO": Capabilities.TURBO,
}

CUSTOM_PRESETS = {
    "FREEZE_PROTECTION": Capabilities.FREEZE_PROTECTION,
}

validate_modes = cv.enum(ALLOWED_CLIMATE_MODES, upper=True)
validate_presets = cv.enum(ALLOWED_CLIMATE_PRESETS, upper=True)
validate_swing_modes = cv.enum(ALLOWED_CLIMATE_SWING_MODES, upper=True)
validate_custom_fan_modes = cv.enum(CUSTOM_FAN_MODES, upper=True)
validate_custom_presets = cv.enum(CUSTOM_PRESETS, upper=True)

CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AirConditioner),
            cv.Optional(CONF_PERIOD, default="1s"): cv.time_period,
            cv.Optional(CONF_TIMEOUT, default="2s"): cv.time_period,
            cv.Optional(CONF_NUM_ATTEMPTS, default=3): cv.int_range(min=1, max=5),
            cv.OnlyWith(CONF_TRANSMITTER_ID, "remote_transmitter"): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
            cv.Optional(CONF_BEEPER, default=False): cv.boolean,
            cv.Optional(CONF_AUTOCONF, default=True): cv.boolean,
            cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(validate_modes),
            cv.Optional(CONF_SUPPORTED_SWING_MODES): cv.ensure_list(
                validate_swing_modes
            ),
            cv.Optional(CONF_SUPPORTED_PRESETS): cv.ensure_list(validate_presets),
            cv.Optional(CONF_CUSTOM_PRESETS): cv.ensure_list(validate_custom_presets),
            cv.Optional(CONF_CUSTOM_FAN_MODES): cv.ensure_list(
                validate_custom_fan_modes
            ),
            cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_USAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY_SETPOINT): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)

# Actions
FollowMeAction = midea_ac_ns.class_("FollowMeAction", automation.Action)
DisplayToggleAction = midea_ac_ns.class_("DisplayToggleAction", automation.Action)
SwingStepAction = midea_ac_ns.class_("SwingStepAction", automation.Action)
BeeperOnAction = midea_ac_ns.class_("BeeperOnAction", automation.Action)
BeeperOffAction = midea_ac_ns.class_("BeeperOffAction", automation.Action)
PowerOnAction = midea_ac_ns.class_("PowerOnAction", automation.Action)
PowerOffAction = midea_ac_ns.class_("PowerOffAction", automation.Action)
PowerToggleAction = midea_ac_ns.class_("PowerToggleAction", automation.Action)

MIDEA_ACTION_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(AirConditioner),
    }
)

# FollowMe action
MIDEA_FOLLOW_ME_MIN = 0
MIDEA_FOLLOW_ME_MAX = 37
MIDEA_FOLLOW_ME_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEMPERATURE): cv.templatable(cv.temperature),
        cv.Optional(CONF_BEEPER, default=False): cv.templatable(cv.boolean),
    }
)


@register_action("follow_me", FollowMeAction, MIDEA_FOLLOW_ME_SCHEMA)
async def follow_me_to_code(var, config, args):
    template_ = await cg.templatable(config[CONF_BEEPER], args, cg.bool_)
    cg.add(var.set_beeper(template_))
    template_ = await cg.templatable(config[CONF_TEMPERATURE], args, cg.float_)
    cg.add(var.set_temperature(template_))


# Toggle Display action
@register_action(
    "display_toggle",
    DisplayToggleAction,
    cv.Schema({}),
)
async def display_toggle_to_code(var, config, args):
    pass


# Swing Step action
@register_action(
    "swing_step",
    SwingStepAction,
    cv.Schema({}),
)
async def swing_step_to_code(var, config, args):
    pass


# Beeper On action
@register_action(
    "beeper_on",
    BeeperOnAction,
    cv.Schema({}),
)
async def beeper_on_to_code(var, config, args):
    pass


# Beeper Off action
@register_action(
    "beeper_off",
    BeeperOffAction,
    cv.Schema({}),
)
async def beeper_off_to_code(var, config, args):
    pass


# Power On action
@register_action(
    "power_on",
    PowerOnAction,
    cv.Schema({}),
)
async def power_on_to_code(var, config, args):
    pass


# Power Off action
@register_action(
    "power_off",
    PowerOffAction,
    cv.Schema({}),
)
async def power_off_to_code(var, config, args):
    pass


# Power Toggle action
@register_action(
    "power_toggle",
    PowerToggleAction,
    cv.Schema({}),
)
async def power_inv_to_code(var, config, args):
    pass


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
    cg.add(var.set_period(config[CONF_PERIOD].total_milliseconds))
    cg.add(var.set_response_timeout(config[CONF_TIMEOUT].total_milliseconds))
    cg.add(var.set_request_attempts(config[CONF_NUM_ATTEMPTS]))
    if CONF_TRANSMITTER_ID in config:
        cg.add_define("USE_REMOTE_TRANSMITTER")
        transmitter_ = await cg.get_variable(config[CONF_TRANSMITTER_ID])
        cg.add(var.set_transmitter(transmitter_))
    cg.add(var.set_beeper_feedback(config[CONF_BEEPER]))
    cg.add(var.set_autoconf(config[CONF_AUTOCONF]))
    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_modes(config[CONF_SUPPORTED_MODES]))
    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
    if CONF_SUPPORTED_PRESETS in config:
        cg.add(var.set_supported_presets(config[CONF_SUPPORTED_PRESETS]))
    if CONF_CUSTOM_PRESETS in config:
        cg.add(var.set_custom_presets(config[CONF_CUSTOM_PRESETS]))
    if CONF_CUSTOM_FAN_MODES in config:
        cg.add(var.set_custom_fan_modes(config[CONF_CUSTOM_FAN_MODES]))
    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature_sensor(sens))
    if CONF_POWER_USAGE in config:
        sens = await sensor.new_sensor(config[CONF_POWER_USAGE])
        cg.add(var.set_power_sensor(sens))
    if CONF_HUMIDITY_SETPOINT in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY_SETPOINT])
        cg.add(var.set_humidity_setpoint_sensor(sens))
    cg.add_library("dudanov/MideaUART", "1.1.8")
