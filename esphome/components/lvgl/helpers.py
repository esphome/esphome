import re

from esphome import config_validation as cv
from esphome.const import CONF_ARGS, CONF_FORMAT

CONF_IF_NAN = "if_nan"

lv_uses = {
    "USER_DATA",
    "LOG",
    "STYLE",
    "FONT_PLACEHOLDER",
    "THEME_DEFAULT",
}


def add_lv_use(*names):
    for name in names:
        lv_uses.add(name)


lv_fonts_used = set()
esphome_fonts_used = set()
lvgl_components_required = set()

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
        lvgl_components_required.add(comp)
        return cv.requires_component(comp)(value)

    return validator
