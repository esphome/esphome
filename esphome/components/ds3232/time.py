import re
from esphome import config_validation as cv, automation, pins
from esphome import codegen as cg
from esphome.components import i2c, time, sensor
from esphome.const import (
    CONF_TEMPERATURE,
    CONF_ID,
    CONF_ACTIVE,
    CONF_TRIGGER_ID,
    CONF_HOUR,
    CONF_MINUTE,
    CONF_SECOND,
    CONF_INTERRUPT_PIN,
    CONF_RESET_PIN,
    CONF_TIME,
    CONF_MODE,
    CONF_TYPE,
    CONF_VALUE,
    CONF_INITIAL_VALUE,
    CONF_SIZE,
    UNIT_CELSIUS,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@ViKozh"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor"]
# Fix for ESPTime wrong location
ESPTime = cg.esphome_ns.struct("ESPTime")

ds3232_ns = cg.esphome_ns.namespace("ds3232")
DS3232Component = ds3232_ns.class_("DS3232Component", time.RealTimeClock, i2c.I2CDevice)
DS3232PowerState = ds3232_ns.enum("DS3232PowerState")

ReadAction = ds3232_ns.class_("ReadAction", automation.Action)
WriteAction = ds3232_ns.class_("WriteAction", automation.Action)
EnableHeartbeatAction = ds3232_ns.class_("EnableHeartbeatAction", automation.Action)
EnableAlarmsAction = ds3232_ns.class_("EnableAlarmsAction", automation.Action)
SetAlarmAction = ds3232_ns.class_("SetAlarmAction", automation.Action)
FactoryResetAction = ds3232_ns.class_("FactoryResetNVRAMAction", automation.Action)
EraseMemoryAction = ds3232_ns.class_("EraseNVRAMAction", automation.Action)

PowerStateEventTrigger = ds3232_ns.class_(
    "PowerStateEvent",
    automation.Trigger.template(DS3232PowerState),
)
HeartBeatEventTrigger = ds3232_ns.class_(
    "HeartbeatEvent", automation.Trigger.template()
)
AlarmFiredEventTrigger = ds3232_ns.class_(
    "AlarmFiredEvent",
    automation.Trigger.template(ESPTime),
)

IsHeartbeatEnabledCondition = ds3232_ns.class_(
    "IsHeartbeatEnabledCondition", automation.Condition
)

ds3232_alarm_ns = ds3232_ns.namespace("ds3232_alarm")
DS3232Alarm = ds3232_alarm_ns.struct("DS3232Alarm")
DS3232AlarmMode = ds3232_alarm_ns.enum("AlarmMode")

ds3232_variables_ns = ds3232_ns.namespace("ds3232_variables")
DS3232Variable = ds3232_variables_ns.class_("DS3232Variable", cg.Component)
DS3232VariableSetAction = ds3232_variables_ns.class_(
    "DS3232VariableSetAction", automation.Action
)

ALARM_MODES = {
    "EVERY_TIME": DS3232AlarmMode.EVERY_TIME,
    "MATCH_SECONDS": DS3232AlarmMode.MATCH_SECONDS,
    "MATCH_MINUTES_SECONDS": DS3232AlarmMode.MATCH_MINUTES_SECONDS,
    "MATCH_TIME": DS3232AlarmMode.MATCH_TIME,
    "MATCH_DATE_AND_TIME": DS3232AlarmMode.MATCH_DATE_AND_TIME,
    "MATCH_DAY_OF_WEEK_AND_TIME": DS3232AlarmMode.MATCH_DAY_OF_WEEK_AND_TIME,
}

WEEKDAYS = {
    "SUN": 1,
    "SUNDAY": 1,
    "MON": 2,
    "MONDAY": 2,
    "TUE": 3,
    "TUESDAY": 3,
    "WEDNESDAY": 4,
    "WED": 4,
    "THURSDAY": 5,
    "THU": 5,
    "FRIDAY": 6,
    "FRI": 6,
    "SATURDAY": 7,
    "SAT": 7,
}

PERSISTENT_VARIABLE_TYPES = {
    "BOOL": ["bool", 1],
    "UINT": ["uint32_t", 4],
    "UINT64": ["uint64_t", 8],
    "UINT32": ["uint32_t", 4],
    "UINT16": ["uint16_t", 2],
    "UINT8": ["uint8_t", 1],
    "INT": ["int32_t", 4],
    "INT64": ["int64_t", 8],
    "INT32": ["int32_t", 4],
    "INT16": ["int16_t", 2],
    "INT8": ["int8_t", 1],
    "FLOAT": ["float", 4],
    "DOUBLE": ["double", 8],
}

CONF_USE_HEARTBEAT = "use_heartbeat"
CONF_FIRE_ALARMS_ON_STARTUP = "fire_alarms_on_startup"
CONF_FIRST_ALARM = "first_alarm"
CONF_SECOND_ALARM = "second_alarm"
CONF_ON_POWER_CHANGE = "on_power_change"
CONF_ON_HEARTBEAT = "on_heartbeat"
CONF_ON_FIRST_ALARM = "on_first_alarm"
CONF_ON_SECOND_ALARM = "on_second_alarm"
CONF_TIME_PATTERN = "time_pattern"
CONF_DAY_OF_MONTH = "day_of_month"
CONF_DAY_OF_WEEK = "day_of_week"
CONF_REGISTER = "register"
CONF_PERSISTENT_STORAGE = "persistent_storage"
CONF_CPP_TYPE = "cpp_type"

MIN_REGISTER_VALUE = 0x18
MAX_REGISTER_VALUE = 0xFF


def validate_variable_definition_(conf):
    size_ = int(PERSISTENT_VARIABLE_TYPES[str.upper(conf[CONF_TYPE])][1])
    end_reg = int(MAX_REGISTER_VALUE) - (size_ - 1)
    if int(MIN_REGISTER_VALUE) <= int(conf[CONF_REGISTER]) <= end_reg:
        conf[CONF_CPP_TYPE] = PERSISTENT_VARIABLE_TYPES[str.upper(conf[CONF_TYPE])][0]
        conf[CONF_SIZE] = size_
    else:
        raise cv.Invalid(
            f"Unable to allocate memory DS3232 in NVRAM for variable {conf[CONF_ID]}"
        )
    return conf


def validate_persistent_storage_(conf):
    memory_map = {x: True for x in range(MIN_REGISTER_VALUE, MAX_REGISTER_VALUE)}
    for var_conf in conf:
        start_reg = int(var_conf[CONF_REGISTER])
        end_reg = start_reg + int(var_conf[CONF_SIZE]) - 1
        subrange = range(start_reg, end_reg)
        if all(memory_map[x] for x in subrange):
            for x in subrange:
                memory_map[x] = False
        else:
            raise cv.Invalid(
                f"Overlapping DS3232 NVRAM addresses found: {hex(start_reg)} to {hex(end_reg)}"
            )
    return conf


PERSISTENT_VARIABLE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(DS3232Variable),
            cv.Required(CONF_TYPE): cv.enum(PERSISTENT_VARIABLE_TYPES, upper=True),
            cv.Required(CONF_REGISTER): cv.hex_int_range(
                min=MIN_REGISTER_VALUE, max=MAX_REGISTER_VALUE
            ),
            cv.Optional(CONF_INITIAL_VALUE): cv.string_strict,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_variable_definition_,
)

VALIDATE_ALARM_REGEX_ = r"^((?P<weekday>\w{3,})|(?P<day>(\d{1,2})|\*)){0,1}\s+(?P<hour>(\d{1,2})|\*):(?P<minute>(\d{1,2})|\*):(?P<second>(\d{1,2})|\*)$"
ALARM_REGEX_PLACEHOLDER_ = "*"


def validate_alarm_(conf):
    value_seconds = 0
    value_minutes = 0
    value_hours = 0
    value_days_of_week = 1
    value_days_of_month = 1
    if CONF_TIME in conf:
        value_seconds = conf[CONF_TIME][CONF_SECOND]
        value_minutes = conf[CONF_TIME][CONF_MINUTE]
        value_hours = conf[CONF_TIME][CONF_HOUR]
        if CONF_DAY_OF_WEEK in conf[CONF_TIME]:
            value_days_of_week = WEEKDAYS.get(
                str.upper(conf[CONF_TIME][CONF_DAY_OF_WEEK])
            )
        if CONF_DAY_OF_MONTH in conf[CONF_TIME]:
            value_days_of_month = conf[CONF_TIME][CONF_DAY_OF_MONTH]
    elif CONF_TIME_PATTERN in conf:
        parse_result = re.fullmatch(
            VALIDATE_ALARM_REGEX_,
            conf[CONF_TIME_PATTERN],
            flags=re.IGNORECASE | re.UNICODE,
        )
        if parse_result:
            values = parse_result.groupdict()
            sec = values.get("second", ALARM_REGEX_PLACEHOLDER_)
            if sec != ALARM_REGEX_PLACEHOLDER_ and sec is not None:
                value_seconds = int(sec)
            min = values.get("minute", ALARM_REGEX_PLACEHOLDER_)
            if min != ALARM_REGEX_PLACEHOLDER_ and min is not None:
                value_minutes = int(min)
            hr = values.get("hour", ALARM_REGEX_PLACEHOLDER_)
            if hr != ALARM_REGEX_PLACEHOLDER_ and hr is not None:
                value_hours = int(hr)
            day_m = values.get("day", ALARM_REGEX_PLACEHOLDER_)
            if day_m != ALARM_REGEX_PLACEHOLDER_ and day_m is not None:
                value_days_of_month = int(day_m)
            weekday = values.get("weekday", ALARM_REGEX_PLACEHOLDER_)
            if weekday != ALARM_REGEX_PLACEHOLDER_ and weekday is not None:
                value_days_of_week = WEEKDAYS.get(str.upper(weekday))
        else:
            raise cv.Invalid(
                "Invalid alarm match pattern. See documentation for reference."
            )
    elif conf[CONF_MODE] != DS3232AlarmMode.EVERY_TIME:
        raise cv.Invalid(
            "Alarms without time settings only possible with EVERY_TIME mode."
        )

    if not (0 <= value_seconds <= 59):
        raise cv.Invalid("Seconds should be in range between 0 and 59.")
    if not (0 <= value_minutes <= 59):
        raise cv.Invalid("Minutes should be in range between 0 and 59.")
    if not (0 <= value_hours <= 59):
        raise cv.Invalid("Hours should be in range between 0 and 23.")
    if not (1 <= value_days_of_month <= 31):
        raise cv.Invalid("Day of month should be in range from 1 to 31.")
    if not (1 <= value_days_of_week <= 7):
        raise cv.Invalid(
            "Day of week should be in range from 1 (Sunday) to 7 (Saturday)."
        )

    conf[CONF_SECOND] = value_seconds
    conf[CONF_MINUTE] = value_minutes
    conf[CONF_HOUR] = value_hours
    conf[CONF_DAY_OF_WEEK] = value_days_of_week
    conf[CONF_DAY_OF_MONTH] = value_days_of_month
    return conf


def validate_(conf):
    if CONF_INTERRUPT_PIN not in conf:
        if CONF_ON_FIRST_ALARM in conf:
            raise cv.Invalid(
                f"'{CONF_INTERRUPT_PIN}' should be defined and properly connected to use '{CONF_ON_FIRST_ALARM}' trigger."
            )
        if CONF_ON_SECOND_ALARM in conf:
            raise cv.Invalid(
                f"'{CONF_INTERRUPT_PIN}' should be defined and properly connected to use '{CONF_ON_SECOND_ALARM}' trigger."
            )
        if CONF_ON_HEARTBEAT in conf:
            raise cv.Invalid(
                f"'{CONF_INTERRUPT_PIN}' should be defined and properly connected to use '{CONF_ON_HEARTBEAT}' trigger."
            )
    if CONF_RESET_PIN not in conf:
        if CONF_ON_POWER_CHANGE in conf:
            raise cv.Invalid(
                f"'{CONF_RESET_PIN}' should be defined and properly connected to use '{CONF_ON_POWER_CHANGE}' trigger."
            )
    return conf


alarm_day_err_msg_ = "Alarm cannot be set both for day of month and day of week."
ALARM_TIME_PATTERN_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SECOND, default=0): cv.Range(
            min=0, min_included=True, max=59, max_included=True
        ),
        cv.Optional(CONF_MINUTE, default=0): cv.Range(
            min=0, min_included=True, max=59, max_included=True
        ),
        cv.Optional(CONF_HOUR, default=0): cv.Range(
            min=0, min_included=True, max=23, max_included=True
        ),
        cv.Exclusive(CONF_DAY_OF_WEEK, "alarm_day", msg=alarm_day_err_msg_): cv.enum(
            WEEKDAYS, upper=True
        ),
        cv.Exclusive(CONF_DAY_OF_MONTH, "alarm_day", msg=alarm_day_err_msg_): cv.Range(
            min=1, min_included=True, max=31, max_included=True
        ),
    }
)

alarm_pattern_err_msg_ = "Alarm should be specified either by time match patterns or time elements. See docs for examples."
ALARM_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MODE): cv.enum(ALARM_MODES, upper=True),
        cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
        cv.Exclusive(
            CONF_TIME_PATTERN, "alarm_pattern", msg=alarm_pattern_err_msg_
        ): cv.string_strict,
        cv.Exclusive(
            CONF_TIME, "alarm_pattern", msg=alarm_pattern_err_msg_
        ): ALARM_TIME_PATTERN_SCHEMA,
    }
)

DS3232_ALARM_SCHEMA = cv.All(ALARM_SCHEMA, validate_alarm_)

CONFIG_SCHEMA = cv.All(
    time.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(DS3232Component),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_USE_HEARTBEAT, default=True): cv.boolean,
            cv.Optional(CONF_INTERRUPT_PIN): pins.gpio_input_pullup_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_FIRE_ALARMS_ON_STARTUP, default=True): cv.boolean,
            cv.Optional(CONF_FIRST_ALARM): DS3232_ALARM_SCHEMA,
            cv.Optional(CONF_SECOND_ALARM): DS3232_ALARM_SCHEMA,
            cv.Optional(CONF_ON_HEARTBEAT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        HeartBeatEventTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_FIRST_ALARM): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        AlarmFiredEventTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_SECOND_ALARM): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        AlarmFiredEventTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_POWER_CHANGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        PowerStateEventTrigger
                    ),
                }
            ),
            cv.Optional(CONF_PERSISTENT_STORAGE, default={}): cv.All(
                [PERSISTENT_VARIABLE_SCHEMA],
                cv.ensure_list(),
                validate_persistent_storage_,
            ),
        }
    ).extend(i2c.i2c_device_schema(0x68)),
    validate_,
)


@automation.register_action(
    "ds3232.factory_reset",
    FactoryResetAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3232Component),
        }
    ),
)
async def ds3232_factory_reset(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3232.erase_memory",
    EraseMemoryAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3232Component),
        }
    ),
)
async def ds3232_erase_memory(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3232.write_time",
    WriteAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3232Component),
        }
    ),
)
async def ds3232_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3232.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3232Component),
        }
    ),
)
async def ds3232_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3232.enable_heartbeat",
    EnableHeartbeatAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3232Component),
        }
    ),
)
async def ds3232_enable_heartbeat_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3232.enable_alarms",
    EnableAlarmsAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3232Component),
        }
    ),
)
async def ds3232_enable_alarms_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_condition(
    "ds3232.is_heartbeat_enabled",
    IsHeartbeatEnabledCondition,
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(DS3232Component)}),
)
async def ds3232_is_hb_enabled_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def alarm(config, is_second_alarm=False):
    is_enabled = cg.bool_(config[CONF_ACTIVE])
    is_seconds_supported = cg.bool_(not is_second_alarm)
    is_fired = cg.bool_(False)
    use_weekdays = None
    day = None
    hour = cg.uint8(config.get(CONF_HOUR, 0))
    minute = cg.uint8(config.get(CONF_MINUTE, 0))
    second = cg.uint8(config.get(CONF_SECOND, 0))
    cur_op = DS3232Alarm.op
    DS3232Alarm.op = "::"
    mode = DS3232Alarm.alarm_mode(
        ALARM_MODES[config.get(CONF_MODE, "MATCH_DAY_OF_WEEK_AND_TIME")]
    )
    DS3232Alarm.op = cur_op
    if config[CONF_MODE] == DS3232AlarmMode.MATCH_DAY_OF_WEEK_AND_TIME:
        use_weekdays = cg.bool_(True)
        day = cg.uint8(config.get(CONF_DAY_OF_WEEK, 1))
    else:
        use_weekdays = cg.bool_(False)
        day = cg.uint8(config.get(CONF_DAY_OF_MONTH, 1))

    cur_op = DS3232Alarm.op
    DS3232Alarm.op = "::"
    expr = DS3232Alarm.create(
        is_enabled,
        mode,
        use_weekdays,
        day,
        hour,
        minute,
        second,
        is_fired,
        is_seconds_supported,
    )
    DS3232Alarm.op = cur_op
    return expr


SET_ALARM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(DS3232Component),
    }
).extend(ALARM_SCHEMA)

SET_ALARM_VALID_SCHEMA = cv.All(
    SET_ALARM_SCHEMA,
    validate_alarm_,
)


# Run with low priority so that namespaces are registered first
@coroutine_with_priority(-100.0)
async def variable_to_code(conf, parent_id):
    var_type = cg.RawExpression(PERSISTENT_VARIABLE_TYPES[conf[CONF_TYPE]][0])
    paren = await cg.get_variable(parent_id)
    template_args = cg.TemplateArguments(var_type)
    register = conf[CONF_REGISTER]
    res_type = DS3232Variable.template(template_args)
    var_instance = None
    if CONF_INITIAL_VALUE in conf:
        var_instance = DS3232Variable.new(
            template_args, paren, register, cg.RawExpression(conf[CONF_INITIAL_VALUE])
        )
    else:
        var_instance = DS3232Variable.new(template_args, paren, register)
    var_persistent = cg.Pvariable(conf[CONF_ID], var_instance, res_type)
    await cg.register_component(var_persistent, conf)


@automation.register_action(
    "ds3232.set_alarm_one", SetAlarmAction, SET_ALARM_VALID_SCHEMA
)
async def ds3232_set_alarm_one(config, action_id, template_args, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    cg.add(var.set_alarm_id(1))
    alarm_ = await alarm(config)
    cg.add(var.set_alarm_data(alarm_))
    return var


@automation.register_action(
    "ds3232.set_alarm_two", SetAlarmAction, SET_ALARM_VALID_SCHEMA
)
async def ds3232_set_alarm_two(config, action_id, template_args, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    cg.add(var.set_alarm_id(2))
    alarm_ = await alarm(config, True)
    cg.add(var.set_alarm_data(alarm_))
    return var


@automation.register_action(
    "ds3232_nvram.set",
    DS3232VariableSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(DS3232Variable),
            cv.Required(CONF_VALUE): cv.templatable(cv.string_strict),
        }
    ),
)
async def ds3232_nvram_set_to_code(config, action_id, template_arg, args):
    full_id, parent = await cg.get_variable_with_full_id(config[CONF_ID])
    template_arg = cg.TemplateArguments(full_id.type, *template_arg)
    variable = cg.new_Pvariable(action_id, template_arg, parent)
    templat = await cg.templatable(
        config[CONF_VALUE], args, None, to_exp=cg.RawExpression
    )
    cg.add(variable.set_value(templat))
    return variable


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)

    for variable in config[CONF_PERSISTENT_STORAGE]:
        await variable_to_code(variable, config[CONF_ID])

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_FIRE_ALARMS_ON_STARTUP in config:
        cg.add(var.set_fire_alarms_on_startup(config[CONF_FIRE_ALARMS_ON_STARTUP]))

    if CONF_INTERRUPT_PIN in config:
        int_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
        cg.add(var.set_interrupt_pin(int_pin))

    for conf in config.get(CONF_ON_FIRST_ALARM, []):
        paren = await cg.get_variable(config[CONF_ID])
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], paren, True)
        await automation.build_automation(
            trigger,
            [
                (ESPTime, "fire_time"),
            ],
            conf,
        )

    for conf in config.get(CONF_ON_SECOND_ALARM, []):
        paren = await cg.get_variable(config[CONF_ID])
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], paren, False)
        await automation.build_automation(
            trigger,
            [
                (ESPTime, "fire_time"),
            ],
            conf,
        )

    if CONF_FIRST_ALARM in config:
        cg.add(
            var.set_alarm_one(
                await alarm(config[CONF_FIRST_ALARM], is_second_alarm=False)
            )
        )

    if CONF_SECOND_ALARM in config:
        cg.add(
            var.set_alarm_two(
                await alarm(config[CONF_SECOND_ALARM], is_second_alarm=True)
            )
        )

    for conf in config.get(CONF_ON_HEARTBEAT, []):
        paren = await cg.get_variable(config[CONF_ID])
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], paren)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_POWER_CHANGE, []):
        paren = await cg.get_variable(config[CONF_ID])
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], paren)
        await automation.build_automation(
            trigger, [(DS3232PowerState, "power_state")], conf
        )
