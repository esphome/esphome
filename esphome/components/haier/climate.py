import logging
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import uart, sensor, climate, logger
from esphome import automation
from esphome.const import (
    CONF_BEEPER,
    CONF_ID,
    CONF_LEVEL,
    CONF_LOGGER,
    CONF_LOGS,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_PROTOCOL,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_SWING_MODES,
    CONF_UART_ID,
    CONF_VISUAL,
    CONF_WIFI,
    DEVICE_CLASS_TEMPERATURE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.components.climate import (
    ClimateSwingMode,
    ClimateMode,
)

_LOGGER = logging.getLogger(__name__)

PROTOCOL_MIN_TEMPERATURE = 16.0
PROTOCOL_MAX_TEMPERATURE = 30.0
PROTOCOL_TEMPERATURE_STEP = 1.0

CODEOWNERS = ["@paveldn"]
AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["climate", "uart"]
CONF_WIFI_SIGNAL = "wifi_signal"
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_VERTICAL_AIRFLOW = "vertical_airflow"
CONF_HORIZONTAL_AIRFLOW = "horizontal_airflow"

PROTOCOLS_SUPPORTED = ["HON", "SMARTAIR2"]

haier_ns = cg.esphome_ns.namespace("haier")
HaierClimateBase = haier_ns.class_(
    "HaierClimateBase", uart.UARTDevice, climate.Climate, cg.Component
)
HonClimate = haier_ns.class_("HonClimate", HaierClimateBase)
Smartair2Climate = haier_ns.class_("Smartair2Climate", HaierClimateBase)


AirflowVerticalDirection = haier_ns.enum("AirflowVerticalDirection")
AIRFLOW_VERTICAL_DIRECTION_OPTIONS = {
    "UP": AirflowVerticalDirection.UP,
    "CENTER": AirflowVerticalDirection.CENTER,
    "DOWN": AirflowVerticalDirection.DOWN,
}

AirflowHorizontalDirection = haier_ns.enum("AirflowHorizontalDirection")
AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS = {
    "LEFT": AirflowHorizontalDirection.LEFT,
    "CENTER": AirflowHorizontalDirection.CENTER,
    "RIGHT": AirflowHorizontalDirection.RIGHT,
}

SUPPORTED_SWING_MODES_OPTIONS = {
    "OFF": ClimateSwingMode.CLIMATE_SWING_OFF,  # always available
    "VERTICAL": ClimateSwingMode.CLIMATE_SWING_VERTICAL,  # always available
    "HORIZONTAL": ClimateSwingMode.CLIMATE_SWING_HORIZONTAL,
    "BOTH": ClimateSwingMode.CLIMATE_SWING_BOTH,
}

SUPPORTED_CLIMATE_MODES_OPTIONS = {
    "OFF": ClimateMode.CLIMATE_MODE_OFF,  # always available
    "AUTO": ClimateMode.CLIMATE_MODE_AUTO,  # always available
    "COOL": ClimateMode.CLIMATE_MODE_COOL,
    "HEAT": ClimateMode.CLIMATE_MODE_HEAT,
    "DRY": ClimateMode.CLIMATE_MODE_DRY,
    "FAN_ONLY": ClimateMode.CLIMATE_MODE_FAN_ONLY,
}


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
    else:
        config[CONF_VISUAL][CONF_MAX_TEMPERATURE] = {
            CONF_MIN_TEMPERATURE: PROTOCOL_MIN_TEMPERATURE,
            CONF_MAX_TEMPERATURE: PROTOCOL_MAX_TEMPERATURE,
        }
    return config


def protocol_config_validation(config):
    ac_protocol = config[CONF_PROTOCOL]
    if CONF_WIFI_SIGNAL in config:
        if ac_protocol == "SMARTAIR2":
            raise cv.Invalid(
                "WiFi signal transmission is not supported by SmartAir2 protocol"
            )
    elif ac_protocol == "HON":
        config[CONF_WIFI_SIGNAL] = True
    if CONF_BEEPER in config:
        if ac_protocol == "SMARTAIR2":
            raise cv.Invalid(
                "Beeper enable/disable is not supported by SmartAir2 protocol implementation"
            )
    elif ac_protocol == "HON":
        config[CONF_BEEPER] = True
    if (CONF_OUTDOOR_TEMPERATURE in config) and (ac_protocol == "SMARTAIR2"):
        raise cv.Invalid(
            "Outdoor temperature sensor is not supported by SmartAir2 protocol"
        )
    return config


CONFIG_SCHEMA = cv.All(
    climate.CLIMATE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HaierClimateBase),
            cv.Required(CONF_PROTOCOL): cv.one_of(*PROTOCOLS_SUPPORTED, upper=True),
            cv.Optional(
                CONF_WIFI_SIGNAL
            ): cv.boolean,  # Default: True for hOn protocol, False for smartAir2 protocol
            cv.Optional(
                CONF_BEEPER
            ): cv.boolean,  # Default: True for hOn protocol, not supported for smartAir2 protocol
            cv.Optional(CONF_SUPPORTED_MODES): cv.ensure_list(
                cv.enum(SUPPORTED_CLIMATE_MODES_OPTIONS, upper=True)
            ),
            cv.Optional(
                CONF_SUPPORTED_SWING_MODES,
                default=[
                    "OFF",
                    "VERTICAL",
                    "HORIZONTAL",
                    "BOTH",
                ],
            ): cv.ensure_list(cv.enum(SUPPORTED_SWING_MODES_OPTIONS, upper=True)),
            cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_visual,
    protocol_config_validation,
    cv.require_esphome_version(2022, 1, 0),
)


# Actions
DisplayOnAction = haier_ns.class_("DisplayOnAction", automation.Action)
DisplayOffAction = haier_ns.class_("DisplayOffAction", automation.Action)
BeeperOnAction = haier_ns.class_("BeeperOnAction", automation.Action)
BeeperOffAction = haier_ns.class_("BeeperOffAction", automation.Action)
VerticalAirflowAction = haier_ns.class_("VerticalAirflowAction", automation.Action)
HorizontalAirflowAction = haier_ns.class_("HorizontalAirflowAction", automation.Action)

HAIER_BASE_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(HaierClimateBase),
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
    "climate.haier.beeper_on", BeeperOnAction, HAIER_BASE_ACTION_SCHEMA
)
@automation.register_action(
    "climate.haier.beeper_off", BeeperOffAction, HAIER_BASE_ACTION_SCHEMA
)
async def beeper_action_to_code(config, action_id, template_arg, args):
    fullid, paren = await cg.get_variable_with_full_id(config[CONF_ID])
    climate_type = str(fullid.type)
    if climate_type != str(HonClimate):
        raise cv.Invalid(
            f'Action "{action_id}" is not supported for type {climate_type}'
        )
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


# Set vertical airflow direction action
@automation.register_action(
    "climate.haier.set_vertical_airflow",
    VerticalAirflowAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(HaierClimateBase),
            cv.Required(CONF_VERTICAL_AIRFLOW): cv.templatable(
                cv.enum(AIRFLOW_VERTICAL_DIRECTION_OPTIONS, upper=True)
            ),
        }
    ),
)
async def haier_set_vertical_airflow_to_code(config, action_id, template_arg, args):
    fullid, paren = await cg.get_variable_with_full_id(config[CONF_ID])
    climate_type = str(fullid.type)
    if climate_type != str(HonClimate):
        raise cv.Invalid(
            f'Action "{action_id}" is not supported for type {climate_type}'
        )
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
            cv.Required(CONF_ID): cv.use_id(HaierClimateBase),
            cv.Required(CONF_HORIZONTAL_AIRFLOW): cv.templatable(
                cv.enum(AIRFLOW_HORIZONTAL_DIRECTION_OPTIONS, upper=True)
            ),
        }
    ),
)
async def haier_set_horizontal_airflow_to_code(config, action_id, template_arg, args):
    fullid, paren = await cg.get_variable_with_full_id(config[CONF_ID])
    climate_type = str(fullid.type)
    if climate_type != str(HonClimate):
        raise cv.Invalid(
            f'Action "{action_id}" is not supported for type {climate_type}'
        )
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(
        config[CONF_HORIZONTAL_AIRFLOW], args, AirflowHorizontalDirection
    )
    cg.add(var.set_direction(template_))
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
    if (
        (CONF_WIFI_SIGNAL in config)
        and (config[CONF_WIFI_SIGNAL])
        and CONF_WIFI not in full_config
    ):
        raise cv.Invalid(
            f"No WiFi configured, if you want to use haier climate without WiFi add {CONF_WIFI_SIGNAL}: false to climate configuration"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    cg.add(haier_ns.init_haier_protocol_logging())
    uart_component = await cg.get_variable(config[CONF_UART_ID])
    ac_protocol = config[CONF_PROTOCOL]
    var = None
    if ac_protocol == "HON":
        rhs = HonClimate.new(uart_component)
        var = cg.Pvariable(config[CONF_ID], rhs, HonClimate)
    elif ac_protocol == "SMARTAIR2":
        rhs = Smartair2Climate.new(uart_component)
        var = cg.Pvariable(config[CONF_ID], rhs, Smartair2Climate)
    else:
        raise cv.Invalid(f"Unknown protocol: {ac_protocol}")
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await climate.register_climate(var, config)
    if (CONF_WIFI_SIGNAL in config) and (config[CONF_WIFI_SIGNAL]):
        cg.add_build_flag("-DHAIER_REPORT_WIFI_SIGNAL")
    if CONF_BEEPER in config:
        cg.add(var.set_beeper_state(config[CONF_BEEPER]))
    if CONF_OUTDOOR_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_OUTDOOR_TEMPERATURE])
        cg.add(var.set_outdoor_temperature_sensor(sens))
    if CONF_SUPPORTED_MODES in config:
        cg.add(var.set_supported_modes(config[CONF_SUPPORTED_MODES]))
    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
    # https://github.com/paveldn/HaierProtocol
    cg.add_library("pavlodn/HaierProtocol", "0.9.18")
