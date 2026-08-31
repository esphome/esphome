from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_HOUR, CONF_ID, CONF_MINUTE, CONF_MODE, CONF_SECOND
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

CODEOWNERS = ["@linkedupbits"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_DS3231_ID = "ds3231_id"
CONF_SQUARE_WAVE_OUTPUT = "square_wave_output"
CONF_BATTERY_BACKED_SQUARE_WAVE = "battery_backed_square_wave"
CONF_ENABLE_32KHZ_OUTPUT = "enable_32khz_output"
CONF_INT_SQW_STARTUP_MODE = "int_sqw_startup_mode"
CONF_ON_ALARM_1 = "on_alarm_1"
CONF_ON_ALARM_2 = "on_alarm_2"
CONF_ALARM = "alarm"
CONF_DAY_OF_WEEK = "day_of_week"
CONF_DAY_OF_MONTH = "day_of_month"
CONF_REFRESH_INTERVAL = "refresh_interval"

# Shared by the switch and select platforms (INT/SQW square-wave control).
ICON_SINE_WAVE = "mdi:sine-wave"

# Optional features are compiled in only when configured, gated behind these defines - see
# https://developers.esphome.io/contributing/code/#gating-optional-features-behind-conditional-compilation
# They are emitted from Python with cg.add_define() here and in the platform __init__.py
# files (whenever a matching entity/action is configured), so esphome/core/defines.h does
# not carry them; clang-tidy simply skips the gated blocks.
USE_DS3231_ALARM = "USE_DS3231_ALARM"
USE_DS3231_SQUARE_WAVE = "USE_DS3231_SQUARE_WAVE"
USE_DS3231_32KHZ_OUTPUT = "USE_DS3231_32KHZ_OUTPUT"
USE_DS3231_AGING_OFFSET = "USE_DS3231_AGING_OFFSET"
USE_DS3231_REFRESH_INTERVAL = "USE_DS3231_REFRESH_INTERVAL"
# Set by the matching platform __init__.py so the hub pulls in binary_sensor.h /
# switch.h / select.h (and its entity pointers) only when that ds3231 platform is used.
USE_DS3231_BINARY_SENSOR = "USE_DS3231_BINARY_SENSOR"
USE_DS3231_SWITCH = "USE_DS3231_SWITCH"
USE_DS3231_SELECT = "USE_DS3231_SELECT"

ds3231_ns = cg.esphome_ns.namespace("ds3231")
DS3231Component = ds3231_ns.class_(
    "DS3231Component", cg.PollingComponent, i2c.I2CDevice
)

# Startup routing of the INT/SQW pin. The select platform can change this at runtime.
INT_SQW_STARTUP_MODE_ALARM = "alarm_interrupt"
INT_SQW_STARTUP_MODE_SQUARE_WAVE = "square_wave"
INT_SQW_STARTUP_MODES = [INT_SQW_STARTUP_MODE_ALARM, INT_SQW_STARTUP_MODE_SQUARE_WAVE]

DS3231SquareWaveFrequency = ds3231_ns.enum("DS3231SquareWaveFrequency", is_class=True)
SQUARE_WAVE_FREQUENCIES = {
    "1Hz": DS3231SquareWaveFrequency.DS3231_SQUARE_WAVE_FREQUENCY_1_HZ,
    "1.024kHz": DS3231SquareWaveFrequency.DS3231_SQUARE_WAVE_FREQUENCY_1024_HZ,
    "4.096kHz": DS3231SquareWaveFrequency.DS3231_SQUARE_WAVE_FREQUENCY_4096_HZ,
    "8.192kHz": DS3231SquareWaveFrequency.DS3231_SQUARE_WAVE_FREQUENCY_8192_HZ,
}

DS3231Alarm1Mode = ds3231_ns.enum("DS3231Alarm1Mode", is_class=True)
ALARM_1_MODES = {
    "EVERY_SECOND": DS3231Alarm1Mode.DS3231_ALARM_1_MODE_EVERY_SECOND,
    "MATCH_SECOND": DS3231Alarm1Mode.DS3231_ALARM_1_MODE_MATCH_SECOND,
    "MATCH_MINUTE_SECOND": DS3231Alarm1Mode.DS3231_ALARM_1_MODE_MATCH_MINUTE_SECOND,
    "MATCH_HOUR_MINUTE_SECOND": DS3231Alarm1Mode.DS3231_ALARM_1_MODE_MATCH_HOUR_MINUTE_SECOND,
    "MATCH_DAY_OF_MONTH": DS3231Alarm1Mode.DS3231_ALARM_1_MODE_MATCH_DAY_OF_MONTH,
    "MATCH_DAY_OF_WEEK": DS3231Alarm1Mode.DS3231_ALARM_1_MODE_MATCH_DAY_OF_WEEK,
}

DS3231Alarm2Mode = ds3231_ns.enum("DS3231Alarm2Mode", is_class=True)
ALARM_2_MODES = {
    "EVERY_MINUTE": DS3231Alarm2Mode.DS3231_ALARM_2_MODE_EVERY_MINUTE,
    "MATCH_MINUTE": DS3231Alarm2Mode.DS3231_ALARM_2_MODE_MATCH_MINUTE,
    "MATCH_HOUR_MINUTE": DS3231Alarm2Mode.DS3231_ALARM_2_MODE_MATCH_HOUR_MINUTE,
    "MATCH_DAY_OF_MONTH": DS3231Alarm2Mode.DS3231_ALARM_2_MODE_MATCH_DAY_OF_MONTH,
    "MATCH_DAY_OF_WEEK": DS3231Alarm2Mode.DS3231_ALARM_2_MODE_MATCH_DAY_OF_WEEK,
}

SetAlarm1Action = ds3231_ns.class_("SetAlarm1Action", automation.Action)
SetAlarm2Action = ds3231_ns.class_("SetAlarm2Action", automation.Action)
ClearAlarmAction = ds3231_ns.class_("ClearAlarmAction", automation.Action)
EnableAlarmAction = ds3231_ns.class_("EnableAlarmAction", automation.Action)
DisableAlarmAction = ds3231_ns.class_("DisableAlarmAction", automation.Action)
ForceTemperatureConversionAction = ds3231_ns.class_(
    "ForceTemperatureConversionAction", automation.Action
)
SetRefreshIntervalAction = ds3231_ns.class_(
    "SetRefreshIntervalAction", automation.Action
)


def _validate(config: ConfigType) -> ConfigType:
    if (
        config[CONF_BATTERY_BACKED_SQUARE_WAVE]
        and CONF_SQUARE_WAVE_OUTPUT not in config
    ):
        raise cv.Invalid(
            f"'{CONF_BATTERY_BACKED_SQUARE_WAVE}' has no effect without "
            f"'{CONF_SQUARE_WAVE_OUTPUT}'",
            path=[CONF_BATTERY_BACKED_SQUARE_WAVE],
        )
    # Default the INT/SQW startup routing: square wave when a frequency is configured,
    # otherwise the alarm-interrupt line.
    if CONF_INT_SQW_STARTUP_MODE not in config:
        config[CONF_INT_SQW_STARTUP_MODE] = (
            INT_SQW_STARTUP_MODE_SQUARE_WAVE
            if CONF_SQUARE_WAVE_OUTPUT in config
            else INT_SQW_STARTUP_MODE_ALARM
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS3231Component),
            cv.Optional(CONF_SQUARE_WAVE_OUTPUT): cv.enum(SQUARE_WAVE_FREQUENCIES),
            cv.Optional(CONF_BATTERY_BACKED_SQUARE_WAVE, default=False): cv.boolean,
            cv.Optional(CONF_INT_SQW_STARTUP_MODE): cv.one_of(
                *INT_SQW_STARTUP_MODES, lower=True
            ),
            cv.Optional(CONF_ENABLE_32KHZ_OUTPUT, default=True): cv.boolean,
            cv.Optional(CONF_ON_ALARM_1): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_ALARM_2): automation.validate_automation(single=True),
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(i2c.i2c_device_schema(0x68)),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if (freq := config.get(CONF_SQUARE_WAVE_OUTPUT)) is not None:
        cg.add_define(USE_DS3231_SQUARE_WAVE)
        cg.add(var.set_square_wave_frequency_config(freq))
        cg.add(
            var.set_battery_backed_square_wave(config[CONF_BATTERY_BACKED_SQUARE_WAVE])
        )

    if config[CONF_INT_SQW_STARTUP_MODE] == INT_SQW_STARTUP_MODE_SQUARE_WAVE:
        cg.add_define(USE_DS3231_SQUARE_WAVE)
        cg.add(var.set_start_in_square_wave_mode(True))

    # The DS3231 powers up with its 32 kHz output enabled, so the default (True) is the
    # hardware behaviour and needs no code; only generate the boot-time write when the
    # config turns it off. (A switch platform, if used, pulls in the feature itself.)
    if not config[CONF_ENABLE_32KHZ_OUTPUT]:
        cg.add_define(USE_DS3231_32KHZ_OUTPUT)
        cg.add(var.set_enable_32khz_output(False))

    for key, method in (
        (CONF_ON_ALARM_1, "add_on_alarm_1_callback"),
        (CONF_ON_ALARM_2, "add_on_alarm_2_callback"),
    ):
        if (conf := config.get(key)) is not None:
            cg.add_define(USE_DS3231_ALARM)
            await automation.build_callback_automation(var, method, [], conf)


_ALARM_1_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DS3231Component),
        cv.Required(CONF_MODE): cv.enum(ALARM_1_MODES, upper=True, space="_"),
        cv.Optional(CONF_SECOND): cv.templatable(cv.int_range(min=0, max=59)),
        cv.Optional(CONF_MINUTE): cv.templatable(cv.int_range(min=0, max=59)),
        cv.Optional(CONF_HOUR): cv.templatable(cv.int_range(min=0, max=23)),
        cv.Optional(CONF_DAY_OF_WEEK): cv.templatable(cv.int_range(min=1, max=7)),
        cv.Optional(CONF_DAY_OF_MONTH): cv.templatable(cv.int_range(min=1, max=31)),
    }
)

_ALARM_2_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DS3231Component),
        cv.Required(CONF_MODE): cv.enum(ALARM_2_MODES, upper=True, space="_"),
        cv.Optional(CONF_MINUTE): cv.templatable(cv.int_range(min=0, max=59)),
        cv.Optional(CONF_HOUR): cv.templatable(cv.int_range(min=0, max=23)),
        cv.Optional(CONF_DAY_OF_WEEK): cv.templatable(cv.int_range(min=1, max=7)),
        cv.Optional(CONF_DAY_OF_MONTH): cv.templatable(cv.int_range(min=1, max=31)),
    }
)


@automation.register_action(
    "ds3231.set_alarm_1",
    SetAlarm1Action,
    cv.All(
        _ALARM_1_SCHEMA,
        cv.has_at_most_one_key(CONF_DAY_OF_WEEK, CONF_DAY_OF_MONTH),
    ),
    synchronous=True,
)
async def ds3231_set_alarm_1_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    cg.add_define(USE_DS3231_ALARM)
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_mode(config[CONF_MODE]))
    await _set_templatable_alarm_fields(var, config, args)
    return var


@automation.register_action(
    "ds3231.set_alarm_2",
    SetAlarm2Action,
    cv.All(
        _ALARM_2_SCHEMA,
        cv.has_at_most_one_key(CONF_DAY_OF_WEEK, CONF_DAY_OF_MONTH),
    ),
    synchronous=True,
)
async def ds3231_set_alarm_2_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    cg.add_define(USE_DS3231_ALARM)
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_mode(config[CONF_MODE]))
    await _set_templatable_alarm_fields(var, config, args)
    return var


async def _set_templatable_alarm_fields(
    var: MockObj, config: ConfigType, args: TemplateArgsType
) -> None:
    for key, setter in (
        (CONF_SECOND, var.set_second),
        (CONF_MINUTE, var.set_minute),
        (CONF_HOUR, var.set_hour),
    ):
        if (value := config.get(key)) is not None:
            templ = await cg.templatable(value, args, cg.uint8)
            cg.add(setter(templ))
    for key in (CONF_DAY_OF_WEEK, CONF_DAY_OF_MONTH):
        if (value := config.get(key)) is not None:
            templ = await cg.templatable(value, args, cg.uint8)
            cg.add(var.set_day(templ))


_ALARM_NUMBER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DS3231Component),
        cv.Required(CONF_ALARM): cv.one_of(1, 2, int=True),
    }
)


async def _alarm_number_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    cg.add_define(USE_DS3231_ALARM)
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_alarm(config[CONF_ALARM]))
    return var


@automation.register_action(
    "ds3231.clear_alarm", ClearAlarmAction, _ALARM_NUMBER_SCHEMA, synchronous=True
)
async def ds3231_clear_alarm_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _alarm_number_action_to_code(config, action_id, template_arg, args)


@automation.register_action(
    "ds3231.enable_alarm", EnableAlarmAction, _ALARM_NUMBER_SCHEMA, synchronous=True
)
async def ds3231_enable_alarm_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _alarm_number_action_to_code(config, action_id, template_arg, args)


@automation.register_action(
    "ds3231.disable_alarm", DisableAlarmAction, _ALARM_NUMBER_SCHEMA, synchronous=True
)
async def ds3231_disable_alarm_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _alarm_number_action_to_code(config, action_id, template_arg, args)


@automation.register_action(
    "ds3231.force_temperature_conversion",
    ForceTemperatureConversionAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3231Component),
        }
    ),
    synchronous=True,
)
async def ds3231_force_temperature_conversion_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3231.set_refresh_interval",
    SetRefreshIntervalAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS3231Component),
            cv.Required(CONF_REFRESH_INTERVAL): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
    synchronous=True,
)
async def ds3231_set_refresh_interval_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    cg.add_define(USE_DS3231_REFRESH_INTERVAL)
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    templ = await cg.templatable(config[CONF_REFRESH_INTERVAL], args, cg.uint32)
    cg.add(var.set_refresh_interval(templ))
    return var
