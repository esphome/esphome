import re

from esphome import config_validation as cv
from esphome.config import Config
from esphome.const import CONF_FORMAT, CONF_ARGS
from esphome.core import ID, CORE
from esphome.yaml_util import ESPHomeDataBase
from .defines import (
    CONF_IMG,
    CONF_ROTARY_ENCODERS,
    CONF_TOUCHSCREENS,
)

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


def mark_line(id: ID):
    if isinstance(id, ESPHomeDataBase):
        path = id.esp_range
    elif isinstance(CORE.config, Config):
        path = CORE.config.get_path_for_id(id)[:-1]
    else:
        return ""
    path = CORE.config.get_deepest_document_range_for_path(path)
    return path.start_mark.as_line_directive


def add_semi(ln: str):
    if ln.startswith("#") or ln.endswith(";"):
        return ln
    return ln + ";"


def join_lines(lines: list, id: ID = None):
    lines = list(map(add_semi, lines))
    if id:
        lines.insert(0, mark_line(id))
    return "\n".join([*lines, ""])
