from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import CONF_ENABLED
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_HOUR,
    CONF_INTERRUPT_PIN,
    CONF_MINUTE,
    CONF_MODE,
    CONF_OFFSET,
    CONF_SECOND,
    CONF_STATE,
)
from esphome.types import ConfigType

CODEOWNERS = ["@linkedupbits"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
DOMAIN = "ds3231"

CONF_DS3231_ID = "ds3231_id"
CONF_ALARM = "alarm"
CONF_ALARM_CHECK_INTERVAL = "alarm_check_interval"
CONF_AGING_OFFSET = "aging_offset"
CONF_AUTO_CLEAR_ALARMS = "auto_clear_alarms"
CONF_BATTERY_BACKED_SQUARE_WAVE = "battery_backed_square_wave"
CONF_DAY_OF_MONTH = "day_of_month"
CONF_DAY_OF_WEEK = "day_of_week"
CONF_ON_ALARM_1 = "on_alarm_1"
CONF_ON_ALARM_2 = "on_alarm_2"
CONF_OUTPUT_32KHZ = "output_32khz"
CONF_SQUARE_WAVE_FREQUENCY = "square_wave_frequency"

ds3231_ns = cg.esphome_ns.namespace("ds3231")
DS3231Component = ds3231_ns.class_("DS3231Component", cg.Component, i2c.I2CDevice)

AlarmMode = ds3231_ns.enum("AlarmMode")
ALARM_1_MODES = AlarmMode.options(
    "every_second",
    "every_minute",
    "hourly",
    "daily",
    "monthly",
    "weekly",
    prefix="ALARM_MODE_",
)
# Alarm 2 has no seconds register.
ALARM_2_MODES = {k: v for k, v in ALARM_1_MODES.items() if k != "every_second"}

SquareWaveFrequency = ds3231_ns.enum("SquareWaveFrequency")
# Keyed by the frequency in Hz; cv.frequency turns "1024Hz", "1.024kHz" or 1024 into it.
SQUARE_WAVE_FREQUENCIES = {
    1: SquareWaveFrequency.SQUARE_WAVE_1HZ,
    1024: SquareWaveFrequency.SQUARE_WAVE_1024HZ,
    4096: SquareWaveFrequency.SQUARE_WAVE_4096HZ,
    8192: SquareWaveFrequency.SQUARE_WAVE_8192HZ,
}
validate_square_wave_frequency = cv.All(
    cv.frequency, cv.enum(SQUARE_WAVE_FREQUENCIES, int=True)
)

validate_aging_offset = cv.int_range(min=-128, max=127)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS3231Component),
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_SQUARE_WAVE_FREQUENCY): validate_square_wave_frequency,
            cv.Optional(CONF_BATTERY_BACKED_SQUARE_WAVE): cv.boolean,
            cv.Optional(CONF_OUTPUT_32KHZ): cv.boolean,
            cv.Optional(CONF_AGING_OFFSET): validate_aging_offset,
            cv.Optional(CONF_AUTO_CLEAR_ALARMS, default=True): cv.boolean,
            cv.Optional(
                CONF_ALARM_CHECK_INTERVAL, default="1s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_ALARM_1): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_ALARM_2): automation.validate_automation(single=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x68)),
    cv.has_at_most_one_key(CONF_INTERRUPT_PIN, CONF_SQUARE_WAVE_FREQUENCY),
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))
    if (frequency := config.get(CONF_SQUARE_WAVE_FREQUENCY)) is not None:
        cg.add(var.set_square_wave_frequency(frequency))
    if (bbsqw := config.get(CONF_BATTERY_BACKED_SQUARE_WAVE)) is not None:
        cg.add(var.set_battery_backed_square_wave(bbsqw))
    if (output_32khz := config.get(CONF_OUTPUT_32KHZ)) is not None:
        cg.add(var.set_output_32khz_config(output_32khz))
    if (aging_offset := config.get(CONF_AGING_OFFSET)) is not None:
        cg.add(var.set_aging_offset_config(aging_offset))
    cg.add(var.set_auto_clear_alarms(config[CONF_AUTO_CLEAR_ALARMS]))
    cg.add(var.set_alarm_check_interval(config[CONF_ALARM_CHECK_INTERVAL]))
    # The alarm flags are only read when something uses them; the binary sensor platform
    # turns this on as well. With an interrupt pin the check also releases INT/SQW.
    if any(
        key in config for key in (CONF_INTERRUPT_PIN, CONF_ON_ALARM_1, CONF_ON_ALARM_2)
    ):
        cg.add(var.set_monitor_alarms(True))

    # One callback for both alarms; each automation runs only for its own alarm number.
    for conf_key, alarm in ((CONF_ON_ALARM_1, 1), (CONF_ON_ALARM_2, 2)):
        if (conf := config.get(conf_key)) is not None:
            await automation.build_callback_automation(
                var,
                "add_on_alarm_callback",
                [],
                conf,
                params=[(cg.uint8, "alarm")],
                forward=[],
                when=f"alarm == {alarm}",
            )


# ---------------------------------------------------------------------------------------------
# Actions
# ---------------------------------------------------------------------------------------------

DS3231_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(DS3231Component)}
)

automation.register_apply_action(
    "ds3231.force_temperature_conversion",
    DS3231_ACTION_SCHEMA,
    automation.ApplyCall("force_temperature_conversion()"),
)


def _validate_alarm_day(config: ConfigType) -> ConfigType:
    mode = config[CONF_MODE]
    if mode == "monthly" and CONF_DAY_OF_MONTH not in config:
        raise cv.Invalid(f"'{CONF_DAY_OF_MONTH}' is required in monthly mode")
    if mode == "weekly" and CONF_DAY_OF_WEEK not in config:
        raise cv.Invalid(f"'{CONF_DAY_OF_WEEK}' is required in weekly mode")
    # The unused day still has to be a valid register value.
    config.setdefault(CONF_DAY_OF_MONTH, 1)
    config.setdefault(CONF_DAY_OF_WEEK, 1)
    return config


def _alarm_schema(modes: dict, with_second: bool) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.use_id(DS3231Component),
        cv.Required(CONF_MODE): cv.enum(modes, lower=True),
        cv.Optional(CONF_MINUTE, default=0): cv.templatable(
            cv.int_range(min=0, max=59)
        ),
        cv.Optional(CONF_HOUR, default=0): cv.templatable(cv.int_range(min=0, max=23)),
        cv.Optional(CONF_DAY_OF_MONTH): cv.templatable(cv.int_range(min=1, max=31)),
        # 1 = Sunday ... 7 = Saturday, as in ESPHome's time component.
        cv.Optional(CONF_DAY_OF_WEEK): cv.templatable(cv.int_range(min=1, max=7)),
        cv.Optional(CONF_ENABLED, default=True): cv.templatable(cv.boolean),
    }
    if with_second:
        schema[cv.Optional(CONF_SECOND, default=0)] = cv.templatable(
            cv.int_range(min=0, max=59)
        )
    return cv.All(cv.Schema(schema), _validate_alarm_day)


automation.register_apply_action(
    "ds3231.set_alarm_1",
    _alarm_schema(ALARM_1_MODES, with_second=True),
    automation.ApplyCall(
        "set_alarm_1({}, {}, {}, {}, {}, {}, {})",
        (
            (CONF_MODE, AlarmMode),
            (CONF_SECOND, cg.uint8),
            (CONF_MINUTE, cg.uint8),
            (CONF_HOUR, cg.uint8),
            (CONF_DAY_OF_MONTH, cg.uint8),
            (CONF_DAY_OF_WEEK, cg.uint8),
            (CONF_ENABLED, cg.bool_),
        ),
    ),
)

automation.register_apply_action(
    "ds3231.set_alarm_2",
    _alarm_schema(ALARM_2_MODES, with_second=False),
    automation.ApplyCall(
        "set_alarm_2({}, {}, {}, {}, {}, {})",
        (
            (CONF_MODE, AlarmMode),
            (CONF_MINUTE, cg.uint8),
            (CONF_HOUR, cg.uint8),
            (CONF_DAY_OF_MONTH, cg.uint8),
            (CONF_DAY_OF_WEEK, cg.uint8),
            (CONF_ENABLED, cg.bool_),
        ),
    ),
)

DS3231_ALARM_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DS3231Component),
        cv.Required(CONF_ALARM): cv.templatable(cv.int_range(min=1, max=2)),
    }
)

for _name, _method in (
    ("ds3231.enable_alarm", "enable_alarm"),
    ("ds3231.disable_alarm", "disable_alarm"),
    ("ds3231.clear_alarm", "clear_alarm"),
):
    automation.register_apply_action(
        _name,
        DS3231_ALARM_ACTION_SCHEMA,
        automation.ApplyField(CONF_ALARM, _method, cg.uint8),
    )

automation.register_apply_condition(
    "ds3231.is_alarm_triggered",
    DS3231_ALARM_ACTION_SCHEMA,
    automation.ApplyCall("is_alarm_triggered({})", ((CONF_ALARM, cg.uint8),)),
)

automation.register_apply_action(
    "ds3231.enable_square_wave",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS3231Component),
            cv.Required(CONF_FREQUENCY): validate_square_wave_frequency,
        }
    ),
    automation.ApplyField(CONF_FREQUENCY, "enable_square_wave", SquareWaveFrequency),
)

automation.register_apply_action(
    "ds3231.disable_square_wave",
    DS3231_ACTION_SCHEMA,
    automation.ApplyCall("disable_square_wave()"),
)

automation.register_apply_action(
    "ds3231.set_32khz_output",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS3231Component),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyField(CONF_STATE, "set_output_32khz", cg.bool_),
)

automation.register_apply_action(
    "ds3231.set_aging_offset",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS3231Component),
            cv.Required(CONF_OFFSET): cv.templatable(validate_aging_offset),
        }
    ),
    automation.ApplyField(CONF_OFFSET, "set_aging_offset", cg.int8),
)
