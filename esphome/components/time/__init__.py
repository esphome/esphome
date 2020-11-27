import bisect
import datetime
import logging
import math
import string

import pytz
import tzlocal

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_CRON, CONF_DAYS_OF_MONTH, CONF_DAYS_OF_WEEK, CONF_HOURS, \
    CONF_MINUTES, CONF_MONTHS, CONF_ON_TIME, CONF_SECONDS, CONF_TIMEZONE, CONF_TRIGGER_ID, \
    CONF_AT, CONF_SECOND, CONF_HOUR, CONF_MINUTE
from esphome.core import coroutine, coroutine_with_priority
from esphome.automation import Condition

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ['@OttoWinter']
IS_PLATFORM_COMPONENT = True

time_ns = cg.esphome_ns.namespace('time')
RealTimeClock = time_ns.class_('RealTimeClock', cg.Component)
CronTrigger = time_ns.class_('CronTrigger', automation.Trigger.template(), cg.Component)
ESPTime = time_ns.struct('ESPTime')
TimeHasTimeCondition = time_ns.class_('TimeHasTimeCondition', Condition)


def _tz_timedelta(td):
    offset_hour = int(td.total_seconds() / (60 * 60))
    offset_minute = int(abs(td.total_seconds() / 60)) % 60
    offset_second = int(abs(td.total_seconds())) % 60
    if offset_hour == 0 and offset_minute == 0 and offset_second == 0:
        return '0'
    if offset_minute == 0 and offset_second == 0:
        return f'{offset_hour}'
    if offset_second == 0:
        return f'{offset_hour}:{offset_minute}'
    return f'{offset_hour}:{offset_minute}:{offset_second}'


# https://stackoverflow.com/a/16804556/8924614
def _week_of_month(dt):
    first_day = dt.replace(day=1)
    dom = dt.day
    adjusted_dom = dom + first_day.weekday()
    return int(math.ceil(adjusted_dom / 7.0))


def _tz_dst_str(dt):
    td = datetime.timedelta(hours=dt.hour, minutes=dt.minute, seconds=dt.second)
    return 'M{}.{}.{}/{}'.format(dt.month, _week_of_month(dt), dt.isoweekday() % 7,
                                 _tz_timedelta(td))


def _safe_tzname(tz, dt):
    tzname = tz.tzname(dt)
    # pytz does not always return valid tznames
    # For example: 'Europe/Saratov' returns '+04'
    # Work around it by using a generic name for the timezone
    if not all(c in string.ascii_letters for c in tzname):
        return 'TZ'
    return tzname


def _non_dst_tz(tz, dt):
    tzname = _safe_tzname(tz, dt)
    utcoffset = tz.utcoffset(dt)
    _LOGGER.info("Detected timezone '%s' with UTC offset %s",
                 tzname, _tz_timedelta(utcoffset))
    tzbase = '{}{}'.format(tzname, _tz_timedelta(-1 * utcoffset))
    return tzbase


def convert_tz(pytz_obj):
    tz = pytz_obj

    now = datetime.datetime.now()
    first_january = datetime.datetime(year=now.year, month=1, day=1)

    if not isinstance(tz, pytz.tzinfo.DstTzInfo):
        return _non_dst_tz(tz, first_january)

    # pylint: disable=protected-access
    transition_times = tz._utc_transition_times
    transition_info = tz._transition_info
    idx = max(0, bisect.bisect_right(transition_times, now))
    if idx >= len(transition_times):
        return _non_dst_tz(tz, now)

    idx1, idx2 = idx, idx + 1
    dstoffset1 = transition_info[idx1][1]
    if dstoffset1 == datetime.timedelta(seconds=0):
        # Normalize to 1 being DST on
        idx1, idx2 = idx + 1, idx + 2

    if idx2 >= len(transition_times):
        return _non_dst_tz(tz, now)

    if transition_times[idx2].year > now.year + 1:
        # Next transition is scheduled after this year
        # Probably a scheduler timezone change.
        return _non_dst_tz(tz, now)

    utcoffset_on, _, tzname_on = transition_info[idx1]
    utcoffset_off, _, tzname_off = transition_info[idx2]
    dst_begins_utc = transition_times[idx1]
    dst_begins_local = dst_begins_utc + utcoffset_off
    dst_ends_utc = transition_times[idx2]
    dst_ends_local = dst_ends_utc + utcoffset_on

    tzbase = '{}{}'.format(tzname_off, _tz_timedelta(-1 * utcoffset_off))

    tzext = '{}{},{},{}'.format(tzname_on, _tz_timedelta(-1 * utcoffset_on),
                                _tz_dst_str(dst_begins_local), _tz_dst_str(dst_ends_local))
    _LOGGER.info("Detected timezone '%s' with UTC offset %s and daylight savings time from "
                 "%s to %s",
                 tzname_off, _tz_timedelta(utcoffset_off),
                 dst_begins_local.strftime("%d %B %X"),
                 dst_ends_local.strftime("%d %B %X"))
    return tzbase + tzext


def detect_tz():
    try:
        tz = tzlocal.get_localzone()
    except pytz.exceptions.UnknownTimeZoneError:
        _LOGGER.warning("Could not auto-detect timezone. Using UTC...")
        return 'UTC'

    return convert_tz(tz)


def _parse_cron_int(value, special_mapping, message):
    special_mapping = special_mapping or {}
    if isinstance(value, str) and value in special_mapping:
        return special_mapping[value]
    try:
        return int(value)
    except ValueError:
        # pylint: disable=raise-missing-from
        raise cv.Invalid(message.format(value))


def _parse_cron_part(part, min_value, max_value, special_mapping):
    if part in ('*', '?'):
        return set(range(min_value, max_value + 1))
    if '/' in part:
        data = part.split('/')
        if len(data) > 2:
            raise cv.Invalid("Can't have more than two '/' in one time expression, got {}"
                             .format(part))
        offset, repeat = data
        offset_n = 0
        if offset:
            offset_n = _parse_cron_int(offset, special_mapping,
                                       "Offset for '/' time expression must be an integer, got {}")

        try:
            repeat_n = int(repeat)
        except ValueError:
            # pylint: disable=raise-missing-from
            raise cv.Invalid("Repeat for '/' time expression must be an integer, got {}"
                             .format(repeat))
        return set(range(offset_n, max_value + 1, repeat_n))
    if '-' in part:
        data = part.split('-')
        if len(data) > 2:
            raise cv.Invalid("Can't have more than two '-' in range time expression '{}'"
                             .format(part))
        begin, end = data
        begin_n = _parse_cron_int(begin, special_mapping, "Number for time range must be integer, "
                                                          "got {}")
        end_n = _parse_cron_int(end, special_mapping, "Number for time range must be integer, "
                                                      "got {}")
        if end_n < begin_n:
            return set(range(end_n, max_value + 1)) | set(range(min_value, begin_n + 1))
        return set(range(begin_n, end_n + 1))

    return {_parse_cron_int(part, special_mapping, "Number for time expression must be an "
                                                   "integer, got {}")}


def cron_expression_validator(name, min_value, max_value, special_mapping=None):
    def validator(value):
        if isinstance(value, list):
            for v in value:
                if not isinstance(v, int):
                    raise cv.Invalid(
                        "Expected integer for {} '{}', got {}".format(v, name, type(v)))
                if v < min_value or v > max_value:
                    raise cv.Invalid(
                        "{} {} is out of range (min={} max={}).".format(name, v, min_value,
                                                                        max_value))
            return list(sorted(value))
        value = cv.string(value)
        values = set()
        for part in value.split(','):
            values |= _parse_cron_part(part, min_value, max_value, special_mapping)
        return validator(list(values))

    return validator


validate_cron_seconds = cron_expression_validator('seconds', 0, 60)
validate_cron_minutes = cron_expression_validator('minutes', 0, 59)
validate_cron_hours = cron_expression_validator('hours', 0, 23)
validate_cron_days_of_month = cron_expression_validator('days of month', 1, 31)
validate_cron_months = cron_expression_validator('months', 1, 12, {
    'JAN': 1, 'FEB': 2, 'MAR': 3, 'APR': 4, 'MAY': 5, 'JUN': 6, 'JUL': 7, 'AUG': 8,
    'SEP': 9, 'OCT': 10, 'NOV': 11, 'DEC': 12
})
validate_cron_days_of_week = cron_expression_validator('days of week', 1, 7, {
    'SUN': 1, 'MON': 2, 'TUE': 3, 'WED': 4, 'THU': 5, 'FRI': 6, 'SAT': 7
})
CRON_KEYS = [CONF_SECONDS, CONF_MINUTES, CONF_HOURS, CONF_DAYS_OF_MONTH, CONF_MONTHS,
             CONF_DAYS_OF_WEEK]


def validate_cron_raw(value):
    value = cv.string(value)
    value = value.split(' ')
    if len(value) != 6:
        raise cv.Invalid("Cron expression must consist of exactly 6 space-separated parts, "
                         "not {}".format(len(value)))
    seconds, minutes, hours, days_of_month, months, days_of_week = value
    return {
        CONF_SECONDS: validate_cron_seconds(seconds),
        CONF_MINUTES: validate_cron_minutes(minutes),
        CONF_HOURS: validate_cron_hours(hours),
        CONF_DAYS_OF_MONTH: validate_cron_days_of_month(days_of_month),
        CONF_MONTHS: validate_cron_months(months),
        CONF_DAYS_OF_WEEK: validate_cron_days_of_week(days_of_week),
    }


def validate_time_at(value):
    value = cv.time_of_day(value)
    return {
        CONF_HOURS: [value[CONF_HOUR]],
        CONF_MINUTES: [value[CONF_MINUTE]],
        CONF_SECONDS: [value[CONF_SECOND]],
        CONF_DAYS_OF_MONTH: validate_cron_days_of_month('*'),
        CONF_MONTHS: validate_cron_months('*'),
        CONF_DAYS_OF_WEEK: validate_cron_days_of_week('*'),
    }


def validate_cron_keys(value):
    if CONF_CRON in value:
        for key in value.keys():
            if key in CRON_KEYS:
                raise cv.Invalid(f"Cannot use option {key} when cron: is specified.")
        if CONF_AT in value:
            raise cv.Invalid("Cannot use option at with cron!")
        cron_ = value[CONF_CRON]
        value = {x: value[x] for x in value if x != CONF_CRON}
        value.update(cron_)
        return value
    if CONF_AT in value:
        for key in value.keys():
            if key in CRON_KEYS:
                raise cv.Invalid(f"Cannot use option {key} when at: is specified.")
        at_ = value[CONF_AT]
        value = {x: value[x] for x in value if x != CONF_AT}
        value.update(at_)
        return value
    return cv.has_at_least_one_key(*CRON_KEYS)(value)


def validate_tz(value):
    value = cv.string_strict(value)

    try:
        pytz_obj = pytz.timezone(value)
    except pytz.UnknownTimeZoneError:  # pylint: disable=broad-except
        return value

    return convert_tz(pytz_obj)


TIME_SCHEMA = cv.Schema({
    cv.Optional(CONF_TIMEZONE, default=detect_tz): validate_tz,
    cv.Optional(CONF_ON_TIME): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CronTrigger),
        cv.Optional(CONF_SECONDS): validate_cron_seconds,
        cv.Optional(CONF_MINUTES): validate_cron_minutes,
        cv.Optional(CONF_HOURS): validate_cron_hours,
        cv.Optional(CONF_DAYS_OF_MONTH): validate_cron_days_of_month,
        cv.Optional(CONF_MONTHS): validate_cron_months,
        cv.Optional(CONF_DAYS_OF_WEEK): validate_cron_days_of_week,
        cv.Optional(CONF_CRON): validate_cron_raw,
        cv.Optional(CONF_AT): validate_time_at,
    }, validate_cron_keys),
})


@coroutine
def setup_time_core_(time_var, config):
    cg.add(time_var.set_timezone(config[CONF_TIMEZONE]))

    for conf in config.get(CONF_ON_TIME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], time_var)

        seconds = conf.get(CONF_SECONDS, list(range(0, 61)))
        cg.add(trigger.add_seconds(seconds))
        minutes = conf.get(CONF_MINUTES, list(range(0, 60)))
        cg.add(trigger.add_minutes(minutes))
        hours = conf.get(CONF_HOURS, list(range(0, 24)))
        cg.add(trigger.add_hours(hours))
        days_of_month = conf.get(CONF_DAYS_OF_MONTH, list(range(1, 32)))
        cg.add(trigger.add_days_of_month(days_of_month))
        months = conf.get(CONF_MONTHS, list(range(1, 13)))
        cg.add(trigger.add_months(months))
        days_of_week = conf.get(CONF_DAYS_OF_WEEK, list(range(1, 8)))
        cg.add(trigger.add_days_of_week(days_of_week))

        yield cg.register_component(trigger, conf)
        yield automation.build_automation(trigger, [], conf)


@coroutine
def register_time(time_var, config):
    yield setup_time_core_(time_var, config)


@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_define('USE_TIME')
    cg.add_global(time_ns.using)


@automation.register_condition('time.has_time', TimeHasTimeCondition, cv.Schema({
    cv.GenerateID(): cv.use_id(RealTimeClock),
}))
def time_has_time_to_code(config, condition_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(condition_id, template_arg, paren)
