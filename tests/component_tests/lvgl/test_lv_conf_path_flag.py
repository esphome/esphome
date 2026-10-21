"""The ``-DLV_CONF_PATH`` build flag carries a path. PlatformIO, the host
build and the native ESP8266 build shell-lex build flags, while the ESP-IDF
and Zephyr builds write each flag into a quoted CMake argument; both must
decode it to one ``-DLV_CONF_PATH="..."`` argument, including when the
project path contains a space."""

from __future__ import annotations

from pathlib import Path
import re
import shlex

import pytest

from esphome.components.lvgl import lv_conf_path_flag


def _cmake_quoted_argument(flag: str) -> str:
    """Decode ``flag`` as CMake decodes a quoted argument: a backslash escapes the next character."""
    return re.sub(r"\\(.)", r"\1", flag)


@pytest.mark.parametrize(
    "build_path",
    [
        "/home/user/.esphome/build/node",
        "/Users/me/with space/.esphome/build/node",
        "/srv/odd 'quote' $(and)/build",
        "C:/Users/Kris Maddens/esphome/.esphome/build/node",
    ],
    ids=["plain", "space", "shell-specials", "windows"],
)
def test_flag_is_one_argument_everywhere(build_path: str) -> None:
    lv_conf_h_file = Path(build_path) / "src" / "lv_conf.h"
    flag = lv_conf_path_flag(lv_conf_h_file)
    expected = f'-DLV_CONF_PATH="{lv_conf_h_file.as_posix()}"'

    assert shlex.split(flag) == [expected]
    assert _cmake_quoted_argument(flag) == expected
