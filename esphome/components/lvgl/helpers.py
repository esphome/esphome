from collections.abc import Callable
import re
from typing import TypeVar

from esphome import config_validation as cv
from esphome.const import CONF_ARGS, CONF_FORMAT

CONF_IF_NAN = "if_nan"

T = TypeVar("T")


def lazy_once(build: Callable[[], T]) -> Callable[[], T]:
    """Return a no-arg callable that runs ``build`` at most once and caches it.

    Used to defer voluptuous schema construction until first validation. Many
    of the lvgl schemas would otherwise be built at module-import time even for
    YAMLs that never reach them.
    """
    cached: list[T] = []

    def get() -> T:
        if not cached:
            cached.append(build())
        return cached[0]

    return get


# noqa
f_regex = re.compile(
    r"""
    (                                   # start of capture group 1
    %                                   # literal "%"
    [-+0 #]{0,5}                        # optional flags
    (?:\d+|\*)?                         # width
    (?:\.(?:\d+|\*))?                   # precision
    (?:hh|h|ll|l|j|z|t|L|w|I|I32|I64)?  # size
    f                                   # type
    )
    """,
    flags=re.VERBOSE,
)
# noqa
c_regex = re.compile(
    r"""
    (                                   # start of capture group 1
    %                                   # literal "%"
    [-+0 #]{0,5}                        # optional flags
    (?:\d+|\*)?                         # width
    (?:\.(?:\d+|\*))?                   # precision
    (?:hh|h|ll|l|j|z|t|L|w|I|I32|I64)?  # size
    [cCdiouxXeEfgGaAnpsSZ]              # type
    )
    """,
    flags=re.VERBOSE,
)


def validate_printf(value):
    format_string = value[CONF_FORMAT]
    matches = c_regex.findall(format_string)
    if len(matches) != len(value[CONF_ARGS]):
        raise cv.Invalid(
            f"Found {len(matches)} printf-patterns ({', '.join(matches)}), but {len(value[CONF_ARGS])} args were given!"
        )

    if value.get(CONF_IF_NAN) and len(f_regex.findall(format_string)) != 1:
        raise cv.Invalid(
            "Use of 'if_nan' requires a single valid printf-pattern of type %f"
        )
    return value
