import re

from esphome import config_validation as cv
from esphome.config import Config
from esphome.const import CONF_ARGS, CONF_FORMAT
from esphome.core import CORE, ID
from esphome.yaml_util import ESPHomeDataBase

from .defines import CONF_IMG, CONF_ROTARY_ENCODERS, CONF_TOUCHSCREENS

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
REQUIRED_COMPONENTS = {
    CONF_IMG: "image",
    CONF_ROTARY_ENCODERS: "rotary_encoder",
    CONF_TOUCHSCREENS: "touchscreen",
}
lvgl_components_required = set()


def validate_printf(value):
    cfmt = r"""
    (                                  # start of capture group 1
    %                                  # literal "%"
    (?:[-+0 #]{0,5})                   # optional flags
    (?:\d+|\*)?                        # width
    (?:\.(?:\d+|\*))?                  # precision
    (?:h|l|ll|w|I|I32|I64)?            # size
    [cCdiouxXeEfgGaAnpsSZ]             # type
    )
    """  # noqa
    matches = re.findall(cfmt, value[CONF_FORMAT], flags=re.X)
    if len(matches) != len(value[CONF_ARGS]):
        raise cv.Invalid(
            f"Found {len(matches)} printf-patterns ({', '.join(matches)}), but {len(value[CONF_ARGS])} args were given!"
        )
    return value


def get_line_marks(value) -> list:
    """
    If possible, return a preprocessor directive to identify the line number where the given id was defined.
    :param id: The id in question
    :return: Either an empty string, or a #line directive
    """
    path = None
    if isinstance(value, ESPHomeDataBase):
        path = value.esp_range
    elif isinstance(value, ID) and isinstance(CORE.config, Config):
        path = CORE.config.get_path_for_id(value)[:-1]
        path = CORE.config.get_deepest_document_range_for_path(path)
    if path is None:
        return []
    return [path.start_mark.as_line_directive]


def add_semi(ln: str):
    if ln.startswith("#") or ln.endswith(";"):
        return ln
    return ln + ";"


def join_lines(lines: list, id: ID = None):
    lines = list(map(add_semi, lines))
    marks = get_line_marks(id)
    marks.extend(lines)
    return "\n".join([*marks, ""])
