import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
    CONF_SECOND,
    CONF_MINUTE,
    CONF_HOUR,
)


CODEOWNERS = ["@nuttytree"]
DEPENDENCIES = ["i2c"]

ds3231_ns = cg.esphome_ns.namespace("ds3231")
DS3231Component = ds3231_ns.class_(
    "DS3231Component", cg.PollingComponent, i2c.I2CDevice
)

Alarm1Type = ds3231_ns.enum("DS3231Alarm1Type")
ALARM_1_TYPE_ENUM = {
    "EVERY_SECOND": Alarm1Type.EVERY_SECOND,
    "EVERY_SECOND_WITH_INTERRUPT": Alarm1Type.EVERY_SECOND_WITH_INTERRUPT,
    "MATCH_SECOND": Alarm1Type.MATCH_SECOND,
    "MATCH_SECOND_WITH_INTERRUPT": Alarm1Type.MATCH_SECOND_WITH_INTERRUPT,
    "MATCH_MINUTE_SECOND": Alarm1Type.MATCH_MINUTE_SECOND,
    "MATCH_MINUTE_SECOND_WITH_INTERRUPT": Alarm1Type.MATCH_MINUTE_SECOND_WITH_INTERRUPT,
    "MATCH_HOUR_MINUTE_SECOND": Alarm1Type.MATCH_HOUR_MINUTE_SECOND,
    "MATCH_HOUR_MINUTE_SECOND_WITH_INTERRUPT": Alarm1Type.MATCH_HOUR_MINUTE_SECOND_WITH_INTERRUPT,
    "MATCH_DAY_OF_MONTH_HOUR_MINUTE_SECOND": Alarm1Type.MATCH_DAY_OF_MONTH_HOUR_MINUTE_SECOND,
    "MATCH_DAY_OF_MONTH_HOUR_MINUTE_SECOND_WITH_INTERRUPT": Alarm1Type.MATCH_DAY_OF_MONTH_HOUR_MINUTE_SECOND_WITH_INTERRUPT,
    "MATCH_DAY_OF_WEEK_HOUR_MINUTE_SECOND": Alarm1Type.MATCH_DAY_OF_WEEK_HOUR_MINUTE_SECOND,
    "MATCH_DAY_OF_WEEK_HOUR_MINUTE_SECOND_WITH_INTERRUPT": Alarm1Type.MATCH_DAY_OF_WEEK_HOUR_MINUTE_SECOND_WITH_INTERRUPT,
}

Alarm2Type = ds3231_ns.enum("DS3231Alarm2Type")
ALARM_2_TYPE_ENUM = {
    "EVERY_MINUTE": Alarm2Type.EVERY_MINUTE,
    "EVERY_MINUTE_WITH_INTERRUPT": Alarm2Type.EVERY_MINUTE_WITH_INTERRUPT,
    "MATCH_MINUTE": Alarm2Type.MATCH_MINUTE,
    "MATCH_MINUTE_WITH_INTERRUPT": Alarm2Type.MATCH_MINUTE_WITH_INTERRUPT,
    "MATCH_HOUR_MINUTE": Alarm2Type.MATCH_HOUR_MINUTE,
    "MATCH_HOUR_MINUTE_WITH_INTERRUPT": Alarm2Type.MATCH_HOUR_MINUTE_WITH_INTERRUPT,
    "MATCH_DAY_OF_MONTH_HOUR_MINUTE": Alarm2Type.MATCH_DAY_OF_MONTH_HOUR_MINUTE,
    "MATCH_DAY_OF_MONTH_HOUR_MINUTE_WITH_INTERRUPT": Alarm2Type.MATCH_DAY_OF_MONTH_HOUR_MINUTE_WITH_INTERRUPT,
    "MATCH_DAY_OF_WEEK_HOUR_MINUTE": Alarm2Type.MATCH_DAY_OF_WEEK_HOUR_MINUTE,
    "MATCH_DAY_OF_WEEK_HOUR_MINUTE_WITH_INTERRUPT": Alarm2Type.MATCH_DAY_OF_WEEK_HOUR_MINUTE_WITH_INTERRUPT,
}

SquareWaveMode = ds3231_ns.enum("DS3231SquareWaveMode")
SQUARE_WAVE_MODE_ENUM = {
    "SQUARE_WAVE": SquareWaveMode.SQUARE_WAVE_MODE,
    "INTERRUPT": SquareWaveMode.INTERRUPT_MODE,
}

SquareWaveFrequency = ds3231_ns.enum("DS3231SquareWaveFrequency")
SQUARE_WAVE_FREQUENCY_ENUM = {
    "FREQUENCY_1_HZ": SquareWaveFrequency.FREQUENCY_1_HZ,
    "FREQUENCY_1024_HZ": SquareWaveFrequency.FREQUENCY_1024_HZ,
    "FREQUENCY_4096_HZ": SquareWaveFrequency.FREQUENCY_4096_HZ,
    "FREQUENCY_8192_HZ": SquareWaveFrequency.FREQUENCY_8192_HZ,
}

# Actions
SetAlarm1Action = ds3231_ns.class_("SetAlarm1Action", automation.Action)
ResetAlarm1Action = ds3231_ns.class_("ResetAlarm1Action", automation.Action)
SetAlarm2Action = ds3231_ns.class_("SetAlarm2Action", automation.Action)
ResetAlarm2Action = ds3231_ns.class_("ResetAlarm2Action", automation.Action)

# Triggers
Alarm1Trigger = ds3231_ns.class_("Alarm1Trigger", automation.Trigger.template())
Alarm2Trigger = ds3231_ns.class_("Alarm2Trigger", automation.Trigger.template())

CONF_ALARM_TYPE = "alarm_type"
CONF_DAY = "day"
CONF_ON_ALARM_1 = "on_alarm_1"
CONF_ON_ALARM_2 = "on_alarm_2"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS3231Component),
            cv.Optional(CONF_ON_ALARM_1): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Alarm1Trigger),
                }
            ),
            cv.Optional(CONF_ON_ALARM_2): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Alarm2Trigger),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x68))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for conf in config.get(CONF_ON_ALARM_1, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_ALARM_2, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


RESET_ALARM_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(DS3231Component),
    }
)


@automation.register_action(
    "ds3231.set_alarm_1",
    SetAlarm1Action,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(DS3231Component),
            cv.Required(CONF_ALARM_TYPE): cv.templatable(
                cv.enum(ALARM_1_TYPE_ENUM, upper=True)
            ),
            cv.Optional(CONF_SECOND, default="0"): cv.int_range(0, 59),
            cv.Optional(CONF_MINUTE, default="0"): cv.int_range(0, 59),
            cv.Optional(CONF_HOUR, default="0"): cv.int_range(0, 23),
            cv.Optional(CONF_DAY, default="1"): cv.int_range(1, 31),
        }
    ),
)
async def set_alarm_1_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    alarm_type = await cg.templatable(config[CONF_ALARM_TYPE], args, Alarm1Type)
    cg.add(var.set_alarm_type(alarm_type))
    second = await cg.templatable(config[CONF_SECOND], args, int)
    cg.add(var.set_second(second))
    minute = await cg.templatable(config[CONF_MINUTE], args, int)
    cg.add(var.set_minute(minute))
    hour = await cg.templatable(config[CONF_HOUR], args, int)
    cg.add(var.set_hour(hour))
    day = await cg.templatable(config[CONF_DAY], args, int)
    cg.add(var.set_day(day))
    return var


@automation.register_action(
    "ds3231.reset_alarm_1", ResetAlarm1Action, RESET_ALARM_SCHEMA
)
async def reset_alarm_1_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3231.set_alarm_2",
    SetAlarm2Action,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(DS3231Component),
            cv.Required(CONF_ALARM_TYPE): cv.templatable(
                cv.enum(ALARM_2_TYPE_ENUM, upper=True)
            ),
            cv.Optional(CONF_MINUTE, default="0"): cv.int_range(0, 59),
            cv.Optional(CONF_HOUR, default="0"): cv.int_range(0, 23),
            cv.Optional(CONF_DAY, default="1"): cv.int_range(1, 31),
        }
    ),
)
async def set_alarm_2_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    alarm_type = await cg.templatable(config[CONF_ALARM_TYPE], args, Alarm2Type)
    cg.add(var.set_alarm_type(alarm_type))
    minute = await cg.templatable(config[CONF_MINUTE], args, int)
    cg.add(var.set_minute(minute))
    hour = await cg.templatable(config[CONF_HOUR], args, int)
    cg.add(var.set_hour(hour))
    day = await cg.templatable(config[CONF_DAY], args, int)
    cg.add(var.set_day(day))
    return var


@automation.register_action(
    "ds3231.reset_alarm_2", ResetAlarm2Action, RESET_ALARM_SCHEMA
)
async def reset_alarm_2_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
