import logging
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import uart, climate, logger
from esphome import automation
from esphome.const import (
    CONF_BEEPER,
    CONF_DISPLAY,
    CONF_ID,
    CONF_LEVEL,
    CONF_LOGGER,
    CONF_LOGS,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_OUTDOOR_TEMPERATURE,
    CONF_PROTOCOL,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_PRESETS,
    CONF_SUPPORTED_SWING_MODES,
    CONF_TARGET_TEMPERATURE,
    CONF_TEMPERATURE_STEP,
    CONF_TRIGGER_ID,
    CONF_VISUAL,
    CONF_WIFI,
)
from esphome.components.climate import (
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
    CONF_CURRENT_TEMPERATURE,
)

_LOGGER = logging.getLogger(__name__)

PROTOCOL_MIN_TEMPERATURE = 16.0
PROTOCOL_MAX_TEMPERATURE = 30.0
PROTOCOL_TARGET_TEMPERATURE_STEP = 1.0
PROTOCOL_CURRENT_TEMPERATURE_STEP = 0.5
PROTOCOL_CONTROL_PACKET_SIZE = 10
PROTOCOL_MIN_SENSORS_PACKET_SIZE = 18
PROTOCOL_DEFAULT_SENSORS_PACKET_SIZE = 22
PROTOCOL_STATUS_MESSAGE_HEADER_SIZE = 0

CODEOWNERS = ["@paveldn"]
DEPENDENCIES = ["climate", "uart"]
CONF_ALTERNATIVE_SWING_CONTROL = "alternative_swing_control"
CONF_ANSWER_TIMEOUT = "answer_timeout"
CONF_CONTROL_METHOD = "control_method"
CONF_CONTROL_PACKET_SIZE = "control_packet_size"
CONF_HORIZONTAL_AIRFLOW = "horizontal_airflow"
CONF_ON_ALARM_START = "on_alarm_start"
CONF_ON_ALARM_END = "on_alarm_end"
CONF_ON_STATUS_MESSAGE = "on_status_message"
CONF_SENSORS_PACKET_SIZE = "sensors_packet_size"
CONF_STATUS_MESSAGE_HEADER_SIZE = "status_message_header_size"
CONF_VERTICAL_AIRFLOW = "vertical_airflow"
CONF_WIFI_SIGNAL = "wifi_signal"

PROTOCOL_HON = "HON"
PROTOCOL_SMARTAIR2 = "SMARTAIR2"

haier_ns = cg.esphome_ns.namespace("haier")
hon_protocol_ns = haier_ns.namespace("hon_protocol")
HaierClimateBase = haier_ns.class_(
    "HaierClimateBase", uart.UARTDevice, climate.Climate, cg.Component
)
HonClimate = haier_ns.class_("HonClimate", HaierClimateBase)
Smartair2Climate = haier_ns.class_("Smartair2Climate", HaierClimateBase)

CONF_HAIER_ID = "haier_id"

AirflowVerticalDirection = hon_protocol_ns.enum("VerticalSwingMode", True)
AIRFLOW_VERTICAL_DIRECTION_OPTIONS = {
    "HEALTH_UP": AirflowVerticalDirection.HEALTH_UP,
    "MAX_UP": AirflowVerticalDirection.MAX_UP,
    "UP": AirflowVerticalDirection.UP,
    "CENTER": AirflowVerticalDirection.CENTER,
    "DOWN": AirflowVerticalDirection.DOWN,
    "HEALTH_DOWN": AirflowVerticalDirection.HEALTH_DOWN,
}

AirflowHorizontalDirection = hon_protocol_ns.enum("HorizontalSwingMode", True)
AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS = {
    "MAX_LEFT": AirflowHorizontalDirection.MAX_LEFT,
    "LEFT": AirflowHorizontalDirection.LEFT,
    "CENTER": AirflowHorizontalDirection.CENTER,
    "RIGHT": AirflowHorizontalDirection.RIGHT,
    "MAX_RIGHT": AirflowHorizontalDirection.MAX_RIGHT,
}

SUPPORTED_SWING_MODES_OPTIONS = {
    "OFF": ClimateSwingMode.CLIMATE_SWING_OFF,
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
}

SUPPORTED_CLIMATE_MODES_OPTIONS = {
    "OFF": ClimateMode.CLIMATE_MODE_OFF,  # always available
    "HEAT_COOL": ClimateMode.CLIMATE_MODE_HEAT_COOL,  # always available
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
}

SUPPORTED_CLIMATE_PRESETS_SMARTAIR2_OPTIONS = {
    "AWAY": ClimatePreset.CLIMATE_PRESET_AWAY,
    "BOOST": ClimatePreset.CLIMATE_PRESET_BOOST,
    "COMFORT": ClimatePreset.CLIMATE_PRESET_COMFORT,
}

SUPPORTED_CLIMATE_PRESETS_HON_OPTIONS = {
    "AWAY": ClimatePreset.CLIMATE_PRESET_AWAY,
    "BOOST": ClimatePreset.CLIMATE_PRESET_BOOST,
    "SLEEP": ClimatePreset.CLIMATE_PRESET_SLEEP,
}

HonControlMethod = haier_ns.enum("HonControlMethod", True)
SUPPORTED_HON_CONTROL_METHODS = {
    "MONITOR_ONLY": HonControlMethod.MONITOR_ONLY,
    "SET_GROUP_PARAMETERS": HonControlMethod.SET_GROUP_PARAMETERS,
    "SET_SINGLE_PARAMETER": HonControlMethod.SET_SINGLE_PARAMETER,
}

HaierAlarmStartTrigger = haier_ns.class_(
    "HaierAlarmStartTrigger",
    automation.Trigger.template(cg.uint8, cg.const_char_ptr),
)

HaierAlarmEndTrigger = haier_ns.class_(
    "HaierAlarmEndTrigger",
    automation.Trigger.template(cg.uint8, cg.const_char_ptr),
)

StatusMessageTrigger = haier_ns.class_(
    "StatusMessageTrigger",
    automation.Trigger.template(cg.const_char_ptr, cg.size_t),
)


def validate_visual(config):
    if CONF_VISUAL in config:
        visual_config = config[CONF_VISUAL]
        if CONF_MIN_TEMPERATURE in visual_config:
            min_temp = visual_config[CONF_MIN_TEMPERATURE]
            if min_temp < PROTOCOL_MIN_TEMPERATURE:
                raise cv.Invalid(
                    f"Configured visual minimum temperature {min_temp} is lower than supported by Haier protocol is {PROTOCOL_MIN_TEMPERATURE}"
                )
        else:
            config[CONF_VISUAL][CONF_MIN_TEMPERATURE] = PROTOCOL_MIN_TEMPERATURE
        if CONF_MAX_TEMPERATURE in visual_config:
            max_temp = visual_config[CONF_MAX_TEMPERATURE]
            if max_temp > PROTOCOL_MAX_TEMPERATURE:
                raise cv.Invalid(
                    f"Configured visual maximum temperature {max_temp} is higher than supported by Haier protocol is {PROTOCOL_MAX_TEMPERATURE}"
                )
        else:
            config[CONF_VISUAL][CONF_MAX_TEMPERATURE] = PROTOCOL_MAX_TEMPERATURE
        if CONF_TEMPERATURE_STEP in visual_config:
            temp_step = config[CONF_VISUAL][CONF_TEMPERATURE_STEP][
                CONF_TARGET_TEMPERATURE
            ]
            if ((int)(temp_step * 2)) / 2 != temp_step:
                raise cv.Invalid(
                    f"Configured visual temperature step {temp_step} is wrong, it should be a multiple of 0.5"
                )
        else:
            config[CONF_VISUAL][CONF_TEMPERATURE_STEP] = {
                CONF_TARGET_TEMPERATURE: PROTOCOL_TARGET_TEMPERATURE_STEP,
                CONF_CURRENT_TEMPERATURE: PROTOCOL_CURRENT_TEMPERATURE_STEP,
            }
    else:
        config[CONF_VISUAL] = {
            CONF_MIN_TEMPERATURE: PROTOCOL_MIN_TEMPERATURE,
            CONF_MAX_TEMPERATURE: PROTOCOL_MAX_TEMPERATURE,
            CONF_TEMPERATURE_STEP: {
                CONF_TARGET_TEMPERATURE: PROTOCOL_TARGET_TEMPERATURE_STEP,
                CONF_CURRENT_TEMPERATURE: PROTOCOL_CURRENT_TEMPERATURE_STEP,
            },
        }
    return config


BASE_CONFIG_SCHEMA = (
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(
                cv.enum(SUPPORTED_CLIMATE_MODES_OPTIONS, upper=True)
            ),
            cv.Optional(
                CONF_SUPPORTED_SWING_MODES,
                default=[
                    "VERTICAL",
                    "HORIZONTAL",
                    "BOTH",
                ],
            ): cv.ensure_list(cv.enum(SUPPORTED_SWING_MODES_OPTIONS, upper=True)),
            cv.Optional(CONF_WIFI_SIGNAL, default=False): cv.boolean,
            cv.Optional(CONF_DISPLAY): cv.boolean,
            cv.Optional(
                CONF_ANSWER_TIMEOUT,
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_STATUS_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StatusMessageTrigger),
                }
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            PROTOCOL_SMARTAIR2: BASE_CONFIG_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(Smartair2Climate),
                    cv.Optional(
                        CONF_ALTERNATIVE_SWING_CONTROL, default=False
                    ): cv.boolean,
                    cv.Optional(
                        CONF_SUPPORTED_PRESETS,
                        default=["BOOST", "COMFORT"],  # No AWAY by default
                    ): cv.ensure_list(
                        cv.enum(SUPPORTED_CLIMATE_PRESETS_SMARTAIR2_OPTIONS, upper=True)
                    ),
                }
            ),
            PROTOCOL_HON: BASE_CONFIG_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(HonClimate),
                    cv.Optional(
                        CONF_CONTROL_METHOD, default="SET_GROUP_PARAMETERS"
                    ): cv.ensure_list(
                        cv.enum(SUPPORTED_HON_CONTROL_METHODS, upper=True)
                    ),
                    cv.Optional(CONF_BEEPER): cv.invalid(
                        f"The {CONF_BEEPER} option is deprecated, use beeper_on/beeper_off actions or beeper switch for a haier platform instead"
                    ),
                    cv.Optional(
                        CONF_CONTROL_PACKET_SIZE, default=PROTOCOL_CONTROL_PACKET_SIZE
                    ): cv.int_range(min=PROTOCOL_CONTROL_PACKET_SIZE, max=50),
                    cv.Optional(
                        CONF_SENSORS_PACKET_SIZE,
                        default=PROTOCOL_DEFAULT_SENSORS_PACKET_SIZE,
                    ): cv.int_range(min=PROTOCOL_MIN_SENSORS_PACKET_SIZE, max=50),
                    cv.Optional(
                        CONF_STATUS_MESSAGE_HEADER_SIZE,
                        default=PROTOCOL_STATUS_MESSAGE_HEADER_SIZE,
                    ): cv.int_range(min=PROTOCOL_STATUS_MESSAGE_HEADER_SIZE),
                    cv.Optional(
                        CONF_SUPPORTED_PRESETS,
                        default=["BOOST", "SLEEP"],  # No AWAY by default
                    ): cv.ensure_list(
                        cv.enum(SUPPORTED_CLIMATE_PRESETS_HON_OPTIONS, upper=True)
                    ),
                    cv.Optional(CONF_OUTDOOR_TEMPERATURE): cv.invalid(
                        f"The {CONF_OUTDOOR_TEMPERATURE} option is deprecated, use a sensor for a haier platform instead"
                    ),
                    cv.Optional(CONF_ON_ALARM_START): automation.validate_automation(
                        {
                            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                                HaierAlarmStartTrigger
                            ),
                        }
                    ),
                    cv.Optional(CONF_ON_ALARM_END): automation.validate_automation(
                        {
                            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                                HaierAlarmEndTrigger
                            ),
                        }
                    ),
                }
            ),
        },
        key=CONF_PROTOCOL,
        default_type=PROTOCOL_SMARTAIR2,
        upper=True,
    ),
    validate_visual,
)


# Actions
DisplayOnAction = haier_ns.class_("DisplayOnAction", automation.Action)
DisplayOffAction = haier_ns.class_("DisplayOffAction", automation.Action)
BeeperOnAction = haier_ns.class_("BeeperOnAction", automation.Action)
BeeperOffAction = haier_ns.class_("BeeperOffAction", automation.Action)
StartSelfCleaningAction = haier_ns.class_("StartSelfCleaningAction", automation.Action)
StartSteriCleaningAction = haier_ns.class_(
    "StartSteriCleaningAction", automation.Action
)
VerticalAirflowAction = haier_ns.class_("VerticalAirflowAction", automation.Action)
HorizontalAirflowAction = haier_ns.class_("HorizontalAirflowAction", automation.Action)
HealthOnAction = haier_ns.class_("HealthOnAction", automation.Action)
HealthOffAction = haier_ns.class_("HealthOffAction", automation.Action)
PowerOnAction = haier_ns.class_("PowerOnAction", automation.Action)
PowerOffAction = haier_ns.class_("PowerOffAction", automation.Action)
PowerToggleAction = haier_ns.class_("PowerToggleAction", automation.Action)

HAIER_BASE_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(HaierClimateBase),
    }
)

HAIER_HON_BASE_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(HonClimate),
    }
)


@automation.register_action(
    "climate.haier.display_on", DisplayOnAction, HAIER_BASE_ACTION_SCHEMA
)
@automation.register_action(
    "climate.haier.display_off", DisplayOffAction, HAIER_BASE_ACTION_SCHEMA
)
async def display_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


@automation.register_action(
    "climate.haier.beeper_on", BeeperOnAction, HAIER_HON_BASE_ACTION_SCHEMA
)
@automation.register_action(
    "climate.haier.beeper_off", BeeperOffAction, HAIER_HON_BASE_ACTION_SCHEMA
)
async def beeper_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


# Start self cleaning or steri-cleaning action action
@automation.register_action(
    "climate.haier.start_self_cleaning",
    StartSelfCleaningAction,
    HAIER_HON_BASE_ACTION_SCHEMA,
)
@automation.register_action(
    "climate.haier.start_steri_cleaning",
    StartSteriCleaningAction,
    HAIER_HON_BASE_ACTION_SCHEMA,
)
async def start_cleaning_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


# Set vertical airflow direction action
@automation.register_action(
    "climate.haier.set_vertical_airflow",
    VerticalAirflowAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HonClimate),
            cv.Required(CONF_VERTICAL_AIRFLOW): cv.templatable(
                cv.enum(AIRFLOW_VERTICAL_DIRECTION_OPTIONS, upper=True)
            ),
        }
    ),
)
async def haier_set_vertical_airflow_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(
        config[CONF_VERTICAL_AIRFLOW], args, AirflowVerticalDirection
    )
    cg.add(var.set_direction(template_))
    return var


# Set horizontal airflow direction action
@automation.register_action(
    "climate.haier.set_horizontal_airflow",
    HorizontalAirflowAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(HonClimate),
            cv.Required(CONF_HORIZONTAL_AIRFLOW): cv.templatable(
                cv.enum(AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS, upper=True)
            ),
        }
    ),
)
async def haier_set_horizontal_airflow_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(
        config[CONF_HORIZONTAL_AIRFLOW], args, AirflowHorizontalDirection
    )
    cg.add(var.set_direction(template_))
    return var


@automation.register_action(
    "climate.haier.health_on", HealthOnAction, HAIER_BASE_ACTION_SCHEMA
)
@automation.register_action(
    "climate.haier.health_off", HealthOffAction, HAIER_BASE_ACTION_SCHEMA
)
async def health_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


@automation.register_action(
    "climate.haier.power_on", PowerOnAction, HAIER_BASE_ACTION_SCHEMA
)
@automation.register_action(
    "climate.haier.power_off", PowerOffAction, HAIER_BASE_ACTION_SCHEMA
)
@automation.register_action(
    "climate.haier.power_toggle", PowerToggleAction, HAIER_BASE_ACTION_SCHEMA
)
async def power_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


def _final_validate(config):
    full_config = fv.full_config.get()
    if CONF_LOGGER in full_config:
        _level = "NONE"
        logger_config = full_config[CONF_LOGGER]
        if CONF_LOGS in logger_config:
            if "haier.protocol" in logger_config[CONF_LOGS]:
                _level = logger_config[CONF_LOGS]["haier.protocol"]
            else:
                _level = logger_config[CONF_LEVEL]
        _LOGGER.info("Detected log level for Haier protocol: %s", _level)
        if _level not in logger.LOG_LEVEL_SEVERITY:
            raise cv.Invalid("Unknown log level for Haier protocol")
        _severity = logger.LOG_LEVEL_SEVERITY.index(_level)
        cg.add_build_flag(f"-DHAIER_LOG_LEVEL={_severity}")
    else:
        _LOGGER.info(
            "No logger component found, logging for Haier protocol is disabled"
        )
        cg.add_build_flag("-DHAIER_LOG_LEVEL=0")
    if config.get(CONF_WIFI_SIGNAL) and CONF_WIFI not in full_config:
        raise cv.Invalid(
            f"No WiFi configured, if you want to use haier climate without WiFi add {CONF_WIFI_SIGNAL}: false to climate configuration"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    cg.add(haier_ns.init_haier_protocol_logging())
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)

    cg.add(var.set_send_wifi(config[CONF_WIFI_SIGNAL]))
    if CONF_CONTROL_METHOD in config:
        cg.add(var.set_control_method(config[CONF_CONTROL_METHOD]))
    if CONF_BEEPER in config:
        cg.add(var.set_beeper_state(config[CONF_BEEPER]))
    if CONF_DISPLAY in config:
        cg.add(var.set_display_state(config[CONF_DISPLAY]))
    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_modes(config[CONF_SUPPORTED_MODES]))
    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
    if CONF_SUPPORTED_PRESETS in config:
        cg.add(var.set_supported_presets(config[CONF_SUPPORTED_PRESETS]))
    if CONF_ANSWER_TIMEOUT in config:
        cg.add(var.set_answer_timeout(config[CONF_ANSWER_TIMEOUT]))
    if CONF_ALTERNATIVE_SWING_CONTROL in config:
        cg.add(
            var.set_alternative_swing_control(config[CONF_ALTERNATIVE_SWING_CONTROL])
        )
    if CONF_CONTROL_PACKET_SIZE in config:
        cg.add(
            var.set_extra_control_packet_bytes_size(
                config[CONF_CONTROL_PACKET_SIZE] - PROTOCOL_CONTROL_PACKET_SIZE
            )
        )
    if CONF_SENSORS_PACKET_SIZE in config:
        cg.add(
            var.set_extra_sensors_packet_bytes_size(
                config[CONF_SENSORS_PACKET_SIZE] - PROTOCOL_MIN_SENSORS_PACKET_SIZE
            )
        )
    if CONF_STATUS_MESSAGE_HEADER_SIZE in config:
        cg.add(
            var.set_status_message_header_size(config[CONF_STATUS_MESSAGE_HEADER_SIZE])
        )
    for conf in config.get(CONF_ON_ALARM_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.uint8, "code"), (cg.const_char_ptr, "message")], conf
        )
    for conf in config.get(CONF_ON_ALARM_END, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.uint8, "code"), (cg.const_char_ptr, "message")], conf
        )
    for conf in config.get(CONF_ON_STATUS_MESSAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.const_char_ptr, "data"), (cg.size_t, "data_size")], conf
        )
    # https://github.com/paveldn/HaierProtocol
    cg.add_library("pavlodn/HaierProtocol", "0.9.31")
