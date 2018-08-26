import datetime
import logging
import math

import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_TIMEZONE
from esphomeyaml.helpers import add, add_job, esphomelib_ns


_LOGGER = logging.getLogger(__name__)

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

time_ns = esphomelib_ns.namespace('time')


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


TIME_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_TIMEZONE, default=detect_tz): cv.string,
})


def setup_time_core_(time_var, config):
    add(time_var.set_timezone(config[CONF_TIMEZONE]))


def setup_time(time_var, config):
    add_job(setup_time_core_, time_var, config)


BUILD_FLAGS = '-DUSE_TIME'
