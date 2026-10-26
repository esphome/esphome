from esphome import automation
import esphome.codegen as cg
from esphome.components import climate, remote_base, remote_transmitter, sensor, uart
from esphome.components.climate import (
    CLIMATE_MODES,
    CLIMATE_PRESETS,
    CLIMATE_SWING_MODES,
)
from esphome.components.remote_base import CONF_TRANSMITTER_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTOCONF,
    CONF_BEEPER,
    CONF_CUSTOM_FAN_MODES,
    CONF_CUSTOM_PRESETS,
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_OUTDOOR_TEMPERATURE,
    CONF_PERIOD,
    CONF_SUPPORTED_MODES,
    CONF_SUPPORTED_PRESETS,
    CONF_SUPPORTED_SWING_MODES,
    CONF_TEMPERATURE,
    CONF_TIMEOUT,
    CONF_USE_FAHRENHEIT,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    ICON_POWER,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_WATT,
)
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@dudanov"]
DEPENDENCIES = ["climate", "uart"]
AUTO_LOAD = ["sensor"]
CONF_POWER_USAGE = "power_usage"
CONF_HUMIDITY_SETPOINT = "humidity_setpoint"
CONF_DISPLAY_CONTROL = "display_control"
midea_ac_ns = cg.esphome_ns.namespace("midea").namespace("ac")
AirConditioner = midea_ac_ns.class_("AirConditioner", climate.Climate, cg.Component)
Capabilities = midea_ac_ns.namespace("Constants")
DisplayControl = midea_ac_ns.enum("DisplayControl", is_class=True)


def templatize(value):
    if isinstance(value, cv.Schema):
        value = value.schema
    ret = {}
    for key, val in value.items():
        ret[key] = cv.templatable(val)
    return cv.Schema(ret)


ALLOWED_CLIMATE_MODES = {
    k: CLIMATE_MODES[k] for k in ("HEAT_COOL", "COOL", "HEAT", "DRY", "FAN_ONLY")
}

ALLOWED_CLIMATE_PRESETS = {k: CLIMATE_PRESETS[k] for k in ("ECO", "BOOST", "SLEEP")}

ALLOWED_CLIMATE_SWING_MODES = {
    k: CLIMATE_SWING_MODES[k] for k in ("BOTH", "VERTICAL", "HORIZONTAL")
}

CUSTOM_FAN_MODES = {
    "SILENT": Capabilities.SILENT,
    "TURBO": Capabilities.TURBO,
}

CUSTOM_PRESETS = {
    "FREEZE_PROTECTION": Capabilities.FREEZE_PROTECTION,
}

# How midea_ac.display_toggle reaches the appliance; see do_display_toggle() for the AUTO rules
DISPLAY_CONTROLS = DisplayControl.options(
    "UART", "AUTO", "IR", prefix="DISPLAY_CONTROL_"
)

validate_modes = cv.enum(ALLOWED_CLIMATE_MODES, upper=True)
validate_presets = cv.enum(ALLOWED_CLIMATE_PRESETS, upper=True)
validate_swing_modes = cv.enum(ALLOWED_CLIMATE_SWING_MODES, upper=True)
validate_custom_fan_modes = cv.enum(CUSTOM_FAN_MODES, upper=True)
validate_custom_presets = cv.enum(CUSTOM_PRESETS, upper=True)
validate_display_control = cv.enum(DISPLAY_CONTROLS, upper=True)


def validate_display_control_with_transmitter(config: ConfigType) -> ConfigType:
    """`display_control: IR` needs a `remote_transmitter` to send through; UART and AUTO do not."""
    if config.get(CONF_DISPLAY_CONTROL) == "IR" and CONF_TRANSMITTER_ID not in config:
        raise cv.Invalid(
            f"'{CONF_DISPLAY_CONTROL}: IR' requires '{CONF_TRANSMITTER_ID}' to be set"
        )
    return config


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(AirConditioner)
    .extend(
        {
            cv.Optional(CONF_PERIOD, default="1s"): cv.time_period,
            cv.Optional(CONF_TIMEOUT, default="2s"): cv.time_period,
            cv.Optional(CONF_NUM_ATTEMPTS, default=3): cv.int_range(min=1, max=5),
            cv.OnlyWith(CONF_TRANSMITTER_ID, "remote_transmitter"): cv.use_id(
                remote_transmitter.RemoteTransmitterComponent
            ),
            cv.Optional(CONF_BEEPER, default=False): cv.boolean,
            cv.Optional(CONF_AUTOCONF, default=True): cv.boolean,
            cv.Optional(CONF_DISPLAY_CONTROL, default="AUTO"): validate_display_control,
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
    validate_display_control_with_transmitter,
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
        ]
    ),
)

MIDEA_ACTION_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(AirConditioner),
    }
)

# FollowMe action
MIDEA_FOLLOW_ME_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEMPERATURE): cv.templatable(cv.temperature),
        cv.Optional(CONF_USE_FAHRENHEIT, default=False): cv.templatable(cv.boolean),
        cv.Optional(CONF_BEEPER, default=False): cv.templatable(cv.boolean),
    }
)


automation.register_apply_action(
    "midea_ac.follow_me",
    templatize(MIDEA_FOLLOW_ME_SCHEMA).extend(MIDEA_ACTION_BASE_SCHEMA),
    automation.ApplyCall(
        "do_follow_me({}, {}, {})",
        (
            (CONF_TEMPERATURE, cg.float_),
            (CONF_USE_FAHRENHEIT, cg.bool_),
            (CONF_BEEPER, cg.bool_),
        ),
    ),
)

for _name, _call in (
    ("midea_ac.display_toggle", "do_display_toggle()"),
    ("midea_ac.swing_step", "do_swing_step()"),
    ("midea_ac.beeper_on", "do_beeper_on()"),
    ("midea_ac.beeper_off", "do_beeper_off()"),
    ("midea_ac.power_on", "do_power_on()"),
    ("midea_ac.power_off", "do_power_off()"),
    ("midea_ac.power_toggle", "do_power_toggle()"),
):
    automation.register_apply_action(
        _name, MIDEA_ACTION_BASE_SCHEMA, automation.ApplyCall(_call)
    )


FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "midea", baud_rate=9600, require_rx=True, require_tx=True
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_period(config[CONF_PERIOD].total_milliseconds))
    cg.add(var.set_response_timeout(config[CONF_TIMEOUT].total_milliseconds))
    cg.add(var.set_request_attempts(config[CONF_NUM_ATTEMPTS]))
    if CONF_TRANSMITTER_ID in config:
        remote_base.request_protocol("midea")  # ir_transmitter.h uses it from C++
        cg.add_define("USE_REMOTE_TRANSMITTER")
        await cg.add_variable(var.set_transmitter, config[CONF_TRANSMITTER_ID])
    cg.add(var.set_beeper_feedback(config[CONF_BEEPER]))
    cg.add(var.set_autoconf(config[CONF_AUTOCONF]))
    cg.add(var.set_display_control(config[CONF_DISPLAY_CONTROL]))
    cg.add_optional(var.set_supported_modes, config.get(CONF_SUPPORTED_MODES))
    if CONF_SUPPORTED_SWING_MODES in config:
        cg.add(var.set_supported_swing_modes(config[CONF_SUPPORTED_SWING_MODES]))
    cg.add_optional(var.set_supported_presets, config.get(CONF_SUPPORTED_PRESETS))
    climate.set_custom_presets(var, config.get(CONF_CUSTOM_PRESETS, []))
    climate.set_custom_fan_modes(var, config.get(CONF_CUSTOM_FAN_MODES, []))
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_OUTDOOR_TEMPERATURE, var.set_outdoor_temperature_sensor)
    await sensors(CONF_POWER_USAGE, var.set_power_sensor)
    await sensors(CONF_HUMIDITY_SETPOINT, var.set_humidity_setpoint_sensor)
    # MideaUART uses the Arduino WiFi API for the network-notify frame
    # (WiFi auto-enables Network via dependency mapping). On ESP-IDF the
    # library talks to esp_wifi directly, so no library entry is needed.
    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("WiFi", None)
    # Using the repository until a release containing ESP-IDF support is published
    cg.add_library(
        name="MideaUART",
        version=None,
        repository="https://github.com/dudanov/MideaUART.git#eeea6c3e9b4474f067054592b435be1c4e466815",
    )
