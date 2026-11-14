"""LibreTiny gets the xPortGetCoreID stand-in only when its effective LT_LOGLEVEL compiles log_d in."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

_FLAG = "-DxPortGetCoreID=int"


@pytest.mark.parametrize(
    ("framework", "expected"),
    [
        ("loglevel: verbose", True),
        ("loglevel: trace", True),
        ("loglevel: debug", True),
        ("loglevel: info", False),
        ("loglevel: warn", False),
        (None, False),
        # framework options override loglevel, as LibreTiny applies them last
        ("options:\n      LT_LOGLEVEL: LT_LEVEL_DEBUG", True),
        ("options:\n      LT_LOGLEVEL: 1", True),
        ("options:\n      LT_LOGLEVEL: 0", True),
        ("options:\n      LT_LOGLEVEL: LT_LEVEL_INFO", False),
        ("options:\n      LT_LOGLEVEL: 3", False),
        ("loglevel: debug\n    options:\n      LT_LOGLEVEL: LT_LEVEL_WARN", False),
        ("loglevel: warn\n    options:\n      LT_LOGLEVEL: LT_LEVEL_DEBUG", True),
    ],
)
def test_core_id_flag_only_at_debug_loglevels(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    framework: str | None,
    expected: bool,
) -> None:
    framework = f"\n  framework:\n    {framework}" if framework else ""
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\n"
        f"bk72xx:\n  board: generic-bk7231n-qfn32-tuya{framework}\n\n"
        "wifi:\n  ssid: test\n  password: testtest12\n\n"
        "web_server:\n"
    )
    generate_main(config)
    assert (_FLAG in CORE.build_flags) is expected
