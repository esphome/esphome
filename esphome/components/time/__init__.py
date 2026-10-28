import errno
import functools
from importlib import resources
import logging

import tzlocal

from esphome import automation
import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import (
    CONF_AT,
    CONF_CRON,
    CONF_DAYS_OF_MONTH,
    CONF_DAYS_OF_WEEK,
    CONF_HOUR,
    CONF_HOURS,
    CONF_MINUTE,
    CONF_MINUTES,
    CONF_MONTHS,
    CONF_ON_TIME,
    CONF_ON_TIME_SYNC,
    CONF_SECOND,
    CONF_SECONDS,
    CONF_TIMEZONE,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_HOST,
    PLATFORM_LN882X,
    PLATFORM_RP2040,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.helpers import cpp_string_escape
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True
DOMAIN = "time"

time_ns = cg.esphome_ns.namespace("time")
RealTimeClock = time_ns.class_("RealTimeClock", cg.PollingComponent)

# C++ types for pre-parsed timezone struct generation
DSTRuleType_cpp = time_ns.enum("DSTRuleType", is_class=True)
DSTRule_cpp = time_ns.struct("DSTRule")
ParsedTimezone_cpp = time_ns.struct("ParsedTimezone")


# Map Python DSTRuleType enum values to C++ enum expressions. Built lazily to
# avoid importing aioesphomeapi (a heavy import) when the time component is only
# auto-loaded for its schema and never reaches code generation.
@functools.cache
def _dst_rule_type_map() -> dict:
    from aioesphomeapi.posix_tz import DSTRuleType as PyDSTRuleType

    return {
        PyDSTRuleType.NONE: DSTRuleType_cpp.NONE,
        PyDSTRuleType.MONTH_WEEK_DAY: DSTRuleType_cpp.MONTH_WEEK_DAY,
        PyDSTRuleType.JULIAN_NO_LEAP: DSTRuleType_cpp.JULIAN_NO_LEAP,
        PyDSTRuleType.DAY_OF_YEAR: DSTRuleType_cpp.DAY_OF_YEAR,
    }


def _load_tzdata(iana_key: str) -> bytes | None:
    # From https://tzdata.readthedocs.io/en/latest/#examples
    if not iana_key:
        return None
    try:
        package_loc, resource = iana_key.rsplit("/", 1)
    except ValueError:
        # Handle top-level timezone entries like "UTC", "GMT"
        package = "tzdata.zoneinfo"
        resource = iana_key
    else:
        package = "tzdata.zoneinfo." + package_loc.replace("/", ".")

    try:
        return (resources.files(package) / resource).read_bytes()
    except (FileNotFoundError, ModuleNotFoundError, IsADirectoryError):
        return None
    except OSError as e:
        # Windows raises EINVAL for paths with NTFS-illegal chars (e.g. '<'/'>'
        # in POSIX TZ strings like "<+08>-8" that validate_tz feeds back here).
        if e.errno == errno.EINVAL:
            return None
        raise


def _extract_tz_string(tzfile: bytes) -> str:
    try:
        return tzfile.split(b"\n")[-2].decode()
    except (IndexError, UnicodeDecodeError):
        _LOGGER.error("Could not determine TZ string. Please report this issue.")
        _LOGGER.exception("tzfile contents: %s", tzfile)
        raise


def detect_tz() -> str | None:
    if CORE.target_platform not in {
        PLATFORM_ESP8266,
        PLATFORM_ESP32,
        PLATFORM_RP2040,
        PLATFORM_BK72XX,
        PLATFORM_RTL87XX,
        PLATFORM_LN882X,
        PLATFORM_HOST,
    }:
        return None
    # Avoids duplicate logger messages when multiple time components are configured
    if cached := CORE.data.setdefault(DOMAIN, {}).get(CONF_TIMEZONE):
        return cached
    iana_key = tzlocal.get_localzone_name()
    if iana_key is None:
        raise EsphomeError(
            "Could not automatically determine timezone, please set timezone manually."
        )
    tzfile = _load_tzdata(iana_key)
    if tzfile is None:
        raise EsphomeError(
            "Could not automatically determine timezone, please set timezone manually."
        )
    ret = _extract_tz_string(tzfile)
    _LOGGER.info("Detected timezone '%s'", iana_key)
    _LOGGER.debug(" -> TZ string %s", ret)
    CORE.data.setdefault(DOMAIN, {})[CONF_TIMEZONE] = ret
    return ret


def _parse_cron_int(value, special_mapping, message):
    special_mapping = special_mapping or {}
    if isinstance(value, str) and value in special_mapping:
        return special_mapping[value]
    try:
        return int(value)
    except ValueError:
        raise cv.Invalid(message.format(value)) from None


def _parse_cron_part(part, min_value, max_value, special_mapping):
    if part in ("*", "?"):
        return set(range(min_value, max_value + 1))
    if "/" in part:
        data = part.split("/")
        if len(data) > 2:
            raise cv.Invalid(
                f"Can't have more than two '/' in one time expression, got {part}"
            )
        offset, repeat = data
        offset_n = min_value
        if offset and offset not in ("*", "?"):
            offset_n = _parse_cron_int(
                offset,
                special_mapping,
                "Offset for '/' time expression must be an integer, got {}",
            )

        try:
            repeat_n = int(repeat)
        except ValueError:
            raise cv.Invalid(
                f"Repeat for '/' time expression must be an integer, got {repeat}"
            ) from None
        return set(range(offset_n, max_value + 1, repeat_n))
    if "-" in part:
        data = part.split("-")
        if len(data) > 2:
            raise cv.Invalid(
                f"Can't have more than two '-' in range time expression '{part}'"
            )
        begin, end = data
        begin_n = _parse_cron_int(
            begin, special_mapping, "Number for time range must be integer, got {}"
        )
        end_n = _parse_cron_int(
            end, special_mapping, "Number for time range must be integer, got {}"
        )
        if end_n < begin_n:
            return set(range(end_n, max_value + 1)) | set(range(min_value, begin_n + 1))
        return set(range(begin_n, end_n + 1))

    return {
        _parse_cron_int(
            part,
            special_mapping,
            "Number for time expression must be an integer, got {}",
        )
    }


def cron_expression_validator(name, min_value, max_value, special_mapping=None):
    def validator(value):
        if isinstance(value, list):
            for v in value:
                if not isinstance(v, int):
                    raise cv.Invalid(
                        f"Expected integer for {v} '{name}', got {type(v)}"
                    )
                if v < min_value or v > max_value:
                    raise cv.Invalid(
                        f"{name} {v} is out of range (min={min_value} max={max_value})."
                    )
            return sorted(value)
        value = cv.string(value)
        values = set()
        for part in value.split(","):
            values |= _parse_cron_part(part, min_value, max_value, special_mapping)
        return validator(list(values))

    return validator


validate_cron_seconds = cron_expression_validator("seconds", 0, 60)
validate_cron_minutes = cron_expression_validator("minutes", 0, 59)
validate_cron_hours = cron_expression_validator("hours", 0, 23)
validate_cron_days_of_month = cron_expression_validator("days of month", 1, 31)
validate_cron_months = cron_expression_validator(
    "months",
    1,
    12,
    {
        "JAN": 1,
        "FEB": 2,
        "MAR": 3,
        "APR": 4,
        "MAY": 5,
        "JUN": 6,
        "JUL": 7,
        "AUG": 8,
        "SEP": 9,
        "OCT": 10,
        "NOV": 11,
        "DEC": 12,
    },
)
validate_cron_days_of_week = cron_expression_validator(
    "days of week",
    1,
    7,
    {"SUN": 1, "MON": 2, "TUE": 3, "WED": 4, "THU": 5, "FRI": 6, "SAT": 7},
)
CRON_KEYS = [
    CONF_SECONDS,
    CONF_MINUTES,
    CONF_HOURS,
    CONF_DAYS_OF_MONTH,
    CONF_MONTHS,
    CONF_DAYS_OF_WEEK,
]


def validate_cron_raw(value):
    value = cv.string(value)
    value = value.split(" ")
    if len(value) != 6:
        raise cv.Invalid(
            f"Cron expression must consist of exactly 6 space-separated parts, not {len(value)}"
        )
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
        CONF_DAYS_OF_MONTH: validate_cron_days_of_month("*"),
        CONF_MONTHS: validate_cron_months("*"),
        CONF_DAYS_OF_WEEK: validate_cron_days_of_week("*"),
    }


def validate_cron_keys(value):
    if CONF_CRON in value:
        for key in value:
            if key in CRON_KEYS:
                raise cv.Invalid(f"Cannot use option {key} when cron: is specified.")
        if CONF_AT in value:
            raise cv.Invalid("Cannot use option at with cron!")
        cron_ = value[CONF_CRON]
        value = {x: value[x] for x in value if x != CONF_CRON}
        value.update(cron_)
        return value
    if CONF_AT in value:
        for key in value:
            if key in CRON_KEYS:
                raise cv.Invalid(f"Cannot use option {key} when at: is specified.")
        at_ = value[CONF_AT]
        value = {x: value[x] for x in value if x != CONF_AT}
        value.update(at_)
        return value
    return cv.has_at_least_one_key(*CRON_KEYS)(value)


# (key, ESPTime member, lowest value, highest value) of each on_time schedule field
_CRON_FIELDS = (
    (CONF_SECONDS, "second", 0, 60),
    (CONF_MINUTES, "minute", 0, 59),
    (CONF_HOURS, "hour", 0, 23),
    (CONF_DAYS_OF_MONTH, "day_of_month", 1, 31),
    (CONF_MONTHS, "month", 1, 12),
    (CONF_DAYS_OF_WEEK, "day_of_week", 1, 7),
)


def cron_field_check(member: str, values: list[int], low: int, high: int) -> str | None:
    """C++ check that ``x.<member>`` is one of ``values``, or None when they cover ``low`` to ``high``.

    The time is already valid, so every field is known to be within its range.
    """
    values = sorted(set(values))
    if values == list(range(low, high + 1)):
        return None
    field = f"x.{member}"
    first, last = values[0], values[-1]
    if first == last:
        return f"{field} == {first}"
    if values == list(range(first, last + 1)):
        if first == low:
            return f"{field} <= {last}"
        if last == high:
            return f"{field} >= {first}"
        return f"{field} >= {first} && {field} <= {last}"
    step = values[1] - first
    if first == low and values == list(range(low, high + 1, step)):
        return (
            f"{field} % {step} == 0" if low == 0 else f"({field} - {low}) % {step} == 0"
        )
    mask = sum(1 << value for value in values)
    suffix = "ULL" if high >= 32 else "U"
    return f"(({mask:#x}{suffix} >> {field}) & 1)"


def _on_time_check(conf: ConfigType) -> str | None:
    checks = [
        check
        for key, member, low, high in _CRON_FIELDS
        if key in conf and (check := cron_field_check(member, conf[key], low, high))
    ]
    return " && ".join(checks) or None


def validate_tz(value: str) -> str:
    value = cv.string_strict(value)

    tzfile = _load_tzdata(value)
    if tzfile is not None:
        value = _extract_tz_string(tzfile)
        is_iana = True
    else:
        is_iana = False

    # Validate that the POSIX TZ string is parseable (skip empty strings)
    if value:
        from aioesphomeapi.posix_tz import parse_posix_tz as parse_posix_tz_python

        try:
            parse_posix_tz_python(value)
        except ValueError as e:
            if is_iana:
                raise cv.Invalid(f"Invalid POSIX timezone string '{value}': {e}") from e
            raise cv.Invalid(
                f"Invalid POSIX timezone string '{value}': {e}. "
                f"If you meant to use an IANA timezone, check the list of valid "
                f"timezones at "
                f"https://en.wikipedia.org/wiki/List_of_tz_database_time_zones"
            ) from e

    return value


TIME_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TIMEZONE): cv.All(
            cv.only_with_framework(["arduino", "esp-idf", "host"]),
            validate_tz,
        ),
        cv.Optional(CONF_ON_TIME): automation.validate_automation(
            {
                cv.Optional(CONF_SECONDS): validate_cron_seconds,
                cv.Optional(CONF_MINUTES): validate_cron_minutes,
                cv.Optional(CONF_HOURS): validate_cron_hours,
                cv.Optional(CONF_DAYS_OF_MONTH): validate_cron_days_of_month,
                cv.Optional(CONF_MONTHS): validate_cron_months,
                cv.Optional(CONF_DAYS_OF_WEEK): validate_cron_days_of_week,
                cv.Optional(CONF_CRON): validate_cron_raw,
                cv.Optional(CONF_AT): validate_time_at,
            },
            validate_cron_keys,
        ),
        cv.Optional(CONF_ON_TIME_SYNC): automation.validate_automation({}),
    }
).extend(
    # ``visibility=ADVANCED`` flags the inherited ``update_interval``
    # field for visual editors — the 15min default is correct for
    # essentially every user, so editors should keep it tucked under
    # "advanced" so it doesn't crowd the form. Validation is
    # unaffected; YAML can override as before.
    cv.polling_component_schema("15min", visibility=cv.Visibility.ADVANCED)
)


def _emit_dst_rule_fields(prefix, rule):
    """Emit field-by-field assignments for a DSTRule to avoid rodata struct blob."""
    cg.add(cg.RawExpression(f"{prefix}.time_seconds = {rule.time_seconds}"))
    cg.add(cg.RawExpression(f"{prefix}.day = {rule.day}"))
    cg.add(cg.RawExpression(f"{prefix}.type = {_dst_rule_type_map()[rule.type]}"))
    cg.add(cg.RawExpression(f"{prefix}.month = {rule.month}"))
    cg.add(cg.RawExpression(f"{prefix}.week = {rule.week}"))
    cg.add(cg.RawExpression(f"{prefix}.day_of_week = {rule.day_of_week}"))


def _emit_parsed_timezone_fields(parsed):
    """Emit field-by-field assignments for a local ParsedTimezone, then set_global_tz().

    Uses individual assignments on a stack variable instead of a struct initializer
    to keep constants as immediate operands in instructions (.irom0.text/flash)
    rather than a const blob in .rodata (which maps to RAM on ESP8266).
    Wrapped in a scope block to allow multiple time platforms in the same build.
    """
    cg.add(cg.RawStatement("{"))
    cg.add(cg.RawExpression("time::ParsedTimezone tz{}"))
    cg.add(cg.RawExpression(f"tz.std_offset_seconds = {parsed.std_offset_seconds}"))
    cg.add(cg.RawExpression(f"tz.dst_offset_seconds = {parsed.dst_offset_seconds}"))
    _emit_dst_rule_fields("tz.dst_start", parsed.dst_start)
    _emit_dst_rule_fields("tz.dst_end", parsed.dst_end)
    cg.add(time_ns.set_global_tz(cg.RawExpression("tz")))
    cg.add(cg.RawStatement("}"))


async def setup_time_core_(time_var, config):
    timezone = config.get(CONF_TIMEZONE)
    # an empty timezone is treated as disabling timezones completely as before
    if timezone is None:
        timezone = detect_tz()
    if timezone:
        cg.add_define("USE_TIME_TIMEZONE")

        if CORE.is_host:
            # Host platform also needs setenv("TZ")/tzset() for libc compatibility
            cg.add(cg.RawExpression(f'setenv("TZ", {cpp_string_escape(timezone)}, 1)'))
            cg.add(cg.RawExpression("tzset()"))

        # Pre-parse at codegen time, emit struct directly
        from aioesphomeapi.posix_tz import parse_posix_tz as parse_posix_tz_python

        try:
            parsed = parse_posix_tz_python(timezone)
        except ValueError as e:
            raise EsphomeError(f"Invalid timezone: {timezone}") from e
        _emit_parsed_timezone_fields(parsed)

    on_time = config.get(CONF_ON_TIME, [])
    if on_time:
        cg.add_define("USE_TIME_TRIGGERS")

    for conf in on_time:
        await automation.build_callback_automation(
            time_var,
            "add_on_time_callback",
            [],
            conf,
            params=[(cg.ESPTime, "x")],
            forward=[],
            when=_on_time_check(conf),
        )

    for conf in config.get(CONF_ON_TIME_SYNC, []):
        await automation.build_callback_automation(
            time_var, "add_on_time_sync_callback", [], conf
        )


async def register_time(time_var, config):
    await setup_time_core_(time_var, config)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    if CORE.using_zephyr:
        zephyr_add_prj_conf("POSIX_CLOCK", True)
        # From sdk-nrf 2.8.0 newlib time() gets the clock through gettimeofday(),
        # which Zephyr only provides with XSI_SINGLE_PROCESS. Without it time()
        # always returns -1.
        if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(2, 8, 0):
            zephyr_add_prj_conf("POSIX_SINGLE_PROCESS", True)
            zephyr_add_prj_conf("XSI_SINGLE_PROCESS", True)
    cg.add_define("USE_TIME")
    cg.add_global(time_ns.using)


automation.register_apply_condition(
    "time.has_time",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(RealTimeClock),
        }
    ),
    "now().is_valid()",
)


# posix_tz.cpp is fully #ifdef'd on USE_TIME_TIMEZONE, set only when a
# timezone is configured or detected.
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {
        "posix_tz.cpp": "USE_TIME_TIMEZONE",
    }
)
