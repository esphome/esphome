import datetime
import logging
import math

import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.const import CONF_CRON, CONF_DAYS_OF_MONTH, CONF_DAYS_OF_WEEK, CONF_HOURS, \
    CONF_MINUTES, CONF_MONTHS, CONF_ON_TIME, CONF_SECONDS, CONF_TIMEZONE, CONF_TRIGGER_ID
from esphomeyaml.helpers import App, NoArg, Pvariable, add, add_job, esphomelib_ns

_LOGGER = logging.getLogger(__name__)

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

time_ns = esphomelib_ns.namespace('time')
CronTrigger = time_ns.CronTrigger


def _tz_timedelta(td):
    offset_hour = int(td.total_seconds() / (60 * 60))
    offset_minute = int(abs(td.total_seconds() / 60)) % 60
    offset_second = int(abs(td.total_seconds())) % 60
    if offset_hour == 0 and offset_minute == 0 and offset_second == 0:
        return '0'
    elif offset_minute == 0 and offset_second == 0:
        return '{}'.format(offset_hour)
    elif offset_second == 0:
        return '{}:{}'.format(offset_hour, offset_minute)
    return '{}:{}:{}'.format(offset_hour, offset_minute, offset_second)


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


def detect_tz():
    try:
        import tzlocal
        import pytz
    except ImportError:
        raise vol.Invalid("No timezone specified and 'tzlocal' not installed. To automatically "
                          "detect the timezone please install tzlocal (pip2 install tzlocal)")
    try:
        tz = tzlocal.get_localzone()
    except pytz.exceptions.UnknownTimeZoneError:
        _LOGGER.warning("Could not auto-detect timezone. Using UTC...")
        return 'UTC'

    def _dst(dt, is_dst):
        try:
            return tz.dst(dt, is_dst=is_dst)
        except TypeError:  # stupid pytz...
            return tz.dst(dt)

    def _tzname(dt, is_dst):
        try:
            return tz.tzname(dt, is_dst=is_dst)
        except TypeError:  # stupid pytz...
            return tz.tzname(dt)

    def _utcoffset(dt, is_dst):
        try:
            return tz.utcoffset(dt, is_dst=is_dst)
        except TypeError:  # stupid pytz...
            return tz.utcoffset(dt)

    dst_begins = None
    dst_tzname = None
    dst_utcoffset = None
    dst_ends = None
    norm_tzname = None
    norm_utcoffset = None

    hour = datetime.timedelta(hours=1)
    this_year = datetime.datetime.now().year
    dt = datetime.datetime(year=this_year, month=1, day=1)
    last_dst = None
    while dt.year == this_year:
        current_dst = _dst(dt, not last_dst)
        is_dst = bool(current_dst)
        if is_dst != last_dst:
            if is_dst:
                dst_begins = dt
                dst_tzname = _tzname(dt, True)
                dst_utcoffset = _utcoffset(dt, True)
            else:
                dst_ends = dt + hour
                norm_tzname = _tzname(dt, False)
                norm_utcoffset = _utcoffset(dt, False)
            last_dst = is_dst
        dt += hour

    tzbase = '{}{}'.format(norm_tzname, _tz_timedelta(-1 * norm_utcoffset))
    if dst_begins is None:
        # No DST in this timezone
        _LOGGER.info("Auto-detected timezone '%s' with UTC offset %s",
                     norm_tzname, _tz_timedelta(norm_utcoffset))
        return tzbase
    tzext = '{}{},{},{}'.format(dst_tzname, _tz_timedelta(-1 * dst_utcoffset),
                                _tz_dst_str(dst_begins), _tz_dst_str(dst_ends))
    _LOGGER.info("Auto-detected timezone '%s' with UTC offset %s and daylight savings time from "
                 "%s to %s",
                 norm_tzname, _tz_timedelta(norm_utcoffset), dst_begins.strftime("%x %X"),
                 dst_ends.strftime("%x %X"))
    return tzbase + tzext


def _parse_cron_int(value, special_mapping, message):
    special_mapping = special_mapping or {}
    if isinstance(value, (str, unicode)) and value in special_mapping:
        return special_mapping[value]
    try:
        return int(value)
    except ValueError:
        raise vol.Invalid(message.format(value))


def _parse_cron_part(part, min_value, max_value, special_mapping):
    if part == '*' or part == '?':
        return set(x for x in range(min_value, max_value + 1))
    if '/' in part:
        data = part.split('/')
        if len(data) > 2:
            raise vol.Invalid(u"Can't have more than two '/' in one time expression, got {}"
                              .format(part))
        offset, repeat = data
        offset_n = 0
        if offset:
            offset_n = _parse_cron_int(offset, special_mapping,
                                       u"Offset for '/' time expression must be an integer, got {}")

        try:
            repeat_n = int(repeat)
        except ValueError:
            raise vol.Invalid(u"Repeat for '/' time expression must be an integer, got {}"
                              .format(repeat))
        return set(x for x in range(offset_n, max_value + 1, repeat_n))
    if '-' in part:
        data = part.split('-')
        if len(data) > 2:
            raise vol.Invalid(u"Can't have more than two '-' in range time expression '{}'"
                              .format(part))
        begin, end = data
        begin_n = _parse_cron_int(begin, special_mapping, u"Number for time range must be integer, "
                                                          u"got {}")
        end_n = _parse_cron_int(end, special_mapping, u"Number for time range must be integer, "
                                                      u"got {}")
        if end_n < begin_n:
            return set(x for x in range(end_n, max_value + 1)) | \
                   set(x for x in range(min_value, begin_n + 1))
        return set(x for x in range(begin_n, end_n + 1))

    return {_parse_cron_int(part, special_mapping, u"Number for time expression must be an "
                                                   u"integer, got {}")}


def cron_expression_validator(name, min_value, max_value, special_mapping=None):
    def validator(value):
        if isinstance(value, list):
            for v in value:
                if not isinstance(v, int):
                    raise vol.Invalid(
                        "Expected integer for {} '{}', got {}".format(v, name, type(v)))
                if v < min_value or v > max_value:
                    raise vol.Invalid(
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
        raise vol.Invalid("Cron expression must consist of exactly 6 space-separated parts, "
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


def validate_cron_keys(value):
    if CONF_CRON in value:
        for key in value.keys():
            if key in CRON_KEYS:
                raise vol.Invalid("Cannot use option {} when cron: is specified.".format(key))
        cron_ = value[CONF_CRON]
        value = {x: value[x] for x in value if x != CONF_CRON}
        value.update(cron_)
        return value
    return cv.has_at_least_one_key(*CRON_KEYS)(value)


TIME_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_TIMEZONE, default=detect_tz): cv.string,
    vol.Optional(CONF_ON_TIME): vol.All(cv.ensure_list, [vol.All(automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(CronTrigger),
        vol.Optional(CONF_SECONDS): validate_cron_seconds,
        vol.Optional(CONF_MINUTES): validate_cron_minutes,
        vol.Optional(CONF_HOURS): validate_cron_hours,
        vol.Optional(CONF_DAYS_OF_MONTH): validate_cron_days_of_month,
        vol.Optional(CONF_MONTHS): validate_cron_months,
        vol.Optional(CONF_DAYS_OF_WEEK): validate_cron_days_of_week,
        vol.Optional(CONF_CRON): validate_cron_raw,
    }), validate_cron_keys)]),
})


def setup_time_core_(time_var, config):
    add(time_var.set_timezone(config[CONF_TIMEZONE]))

    for conf in config.get(CONF_ON_TIME, []):
        rhs = App.register_component(time_var.Pmake_cron_trigger())
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        for second in conf.get(CONF_SECONDS, [x for x in range(0, 61)]):
            add(trigger.add_second(second))
        for minute in conf.get(CONF_MINUTES, [x for x in range(0, 60)]):
            add(trigger.add_minute(minute))
        for hour in conf.get(CONF_HOURS, [x for x in range(0, 24)]):
            add(trigger.add_hour(hour))
        for day_of_month in conf.get(CONF_DAYS_OF_MONTH, [x for x in range(1, 32)]):
            add(trigger.add_day_of_month(day_of_month))
        for month in conf.get(CONF_MONTHS, [x for x in range(1, 13)]):
            add(trigger.add_month(month))
        for day_of_week in conf.get(CONF_DAYS_OF_WEEK, [x for x in range(1, 8)]):
            add(trigger.add_day_of_week(day_of_week))
        automation.build_automation(trigger, NoArg, conf)


def setup_time(time_var, config):
    add_job(setup_time_core_, time_var, config)


BUILD_FLAGS = '-DUSE_TIME'
