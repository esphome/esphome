import math
from collections import OrderedDict


class ESPHomeYAMLError(Exception):
    """General esphomeyaml exception occurred."""
    pass


class HexInt(long):
    def __str__(self):
        return "0x{:X}".format(self)


class IPAddress(object):
    def __init__(self, *args):
        if len(args) != 4:
            raise ValueError(u"IPAddress must consist up 4 items")
        self.args = args

    def __str__(self):
        return '.'.join(str(x) for x in self.args)


class MACAddress(object):
    def __init__(self, *parts):
        if len(parts) != 6:
            raise ValueError(u"MAC Address must consist of 6 items")
        self.parts = parts

    def __str__(self):
        return ':'.join('{:02X}'.format(part) for part in self.parts)


def is_approximately_integer(value):
    if isinstance(value, (int, long)):
        return True
    return abs(value - round(value)) < 0.001


class TimePeriod(object):
    def __init__(self, microseconds=None, milliseconds=None, seconds=None,
                 minutes=None, hours=None, days=None):
        if days is not None:
            if not is_approximately_integer(days):
                frac_days, days = math.modf(days)
                hours = (hours or 0) + frac_days * 24
            self.days = int(round(days))
        else:
            self.days = None

        if hours is not None:
            if not is_approximately_integer(hours):
                frac_hours, hours = math.modf(hours)
                minutes = (minutes or 0) + frac_hours * 60
            self.hours = int(round(hours))
        else:
            self.hours = None

        if minutes is not None:
            if not is_approximately_integer(minutes):
                frac_minutes, minutes = math.modf(minutes)
                seconds = (seconds or 0) + frac_minutes * 60
            self.minutes = int(round(minutes))
        else:
            self.minutes = None

        if seconds is not None:
            if not is_approximately_integer(seconds):
                frac_seconds, seconds = math.modf(seconds)
                milliseconds = (milliseconds or 0) + frac_seconds * 1000
            self.seconds = int(round(seconds))
        else:
            self.seconds = None

        if milliseconds is not None:
            if not is_approximately_integer(milliseconds):
                frac_milliseconds, milliseconds = math.modf(milliseconds)
                microseconds = (microseconds or 0) + frac_milliseconds * 1000
            self.milliseconds = int(round(milliseconds))
        else:
            self.milliseconds = None

        if microseconds is not None:
            if not is_approximately_integer(microseconds):
                raise ValueError("Maximum precision is microseconds")
            self.microseconds = int(round(microseconds))
        else:
            self.microseconds = None

    def as_dict(self):
        out = OrderedDict()
        if self.microseconds is not None:
            out['microseconds'] = self.microseconds
        if self.milliseconds is not None:
            out['milliseconds'] = self.milliseconds
        if self.seconds is not None:
            out['seconds'] = self.seconds
        if self.minutes is not None:
            out['minutes'] = self.minutes
        if self.hours is not None:
            out['hours'] = self.hours
        if self.days is not None:
            out['days'] = self.days
        return out

    @property
    def total_microseconds(self):
        return self.total_milliseconds * 1000 + (self.microseconds or 0)

    @property
    def total_milliseconds(self):
        return self.total_seconds * 1000 + (self.milliseconds or 0)

    @property
    def total_seconds(self):
        return self.total_minutes * 60 + (self.seconds or 0)

    @property
    def total_minutes(self):
        return self.total_hours * 60 + (self.minutes or 0)

    @property
    def total_hours(self):
        return self.total_days * 24 + (self.hours or 0)

    @property
    def total_days(self):
        return self.days or 0

    def __eq__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds == other.total_microseconds

    def __ne__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds != other.total_microseconds

    def __lt__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds < other.total_microseconds

    def __gt__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds > other.total_microseconds

    def __le__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds <= other.total_microseconds

    def __ge__(self, other):
        if not isinstance(other, TimePeriod):
            raise ValueError("other must be TimePeriod")
        return self.total_microseconds >= other.total_microseconds


class TimePeriodMicroseconds(TimePeriod):
    pass


class TimePeriodMilliseconds(TimePeriod):
    pass


class TimePeriodSeconds(TimePeriod):
    pass


class Lambda(object):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return u'Lambda<{}>'.format(self.value)


CONFIG_PATH = None
SIMPLIFY = True
ESP_PLATFORM = ''
BOARD = ''
RAW_CONFIG = None
