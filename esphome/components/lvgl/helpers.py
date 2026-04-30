import re

from esphome import config_validation as cv
from esphome.const import CONF_ARGS, CONF_FORMAT
from esphome.core import CORE

CONF_IF_NAN = "if_nan"

# Initial set of LVGL features that are always enabled.
_INITIAL_LV_USES = frozenset(
    {
        "USER_DATA",
        "LOG",
        "STYLE",
        "FONT_PLACEHOLDER",
        "THEME_DEFAULT",
    }
)


# These collections accumulate state across a single compilation run.  They
# are stored under ``CORE.data`` (which ``CORE.reset()`` clears between runs)
# rather than as module-level globals, otherwise they would leak between
# successive compilations / unit tests.
def _get_data(key: str, default):
    # Lazy import to avoid a circular dependency with ``defines``.
    from .defines import DOMAIN

    return CORE.data.setdefault(DOMAIN, {}).setdefault(key, default)


def get_lv_uses() -> set:
    from .defines import KEY_LV_USES

    return _get_data(KEY_LV_USES, set(_INITIAL_LV_USES))


def get_lv_fonts_used() -> set:
    from .defines import KEY_LV_FONTS_USED

    return _get_data(KEY_LV_FONTS_USED, set())


def get_esphome_fonts_used() -> set:
    from .defines import KEY_ESPHOME_FONTS_USED

    return _get_data(KEY_ESPHOME_FONTS_USED, set())


def add_lv_use(*names):
    uses = get_lv_uses()
    for name in names:
        uses.add(name)


# noqa
f_regex = re.compile(
    r"""
    (                                  # start of capture group 1
    %                                  # literal "%"
    [-+0 #]{0,5}                   # optional flags
    (?:\d+|\*)?                        # width
    (?:\.(?:\d+|\*))?                  # precision
    (?:h|l|ll|w|I|I32|I64)?            # size
    f                                  # type
    )
    """,
    flags=re.VERBOSE,
)
# noqa
c_regex = re.compile(
    r"""
    (                                  # start of capture group 1
    %                                  # literal "%"
    [-+0 #]{0,5}                   # optional flags
    (?:\d+|\*)?                        # width
    (?:\.(?:\d+|\*))?                  # precision
    (?:h|l|ll|w|I|I32|I64)?            # size
    [cCdiouxXeEfgGaAnpsSZ]             # type
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


def requires_component(comp):
    def validator(value):
        return cv.requires_component(comp)(value)

    return validator
