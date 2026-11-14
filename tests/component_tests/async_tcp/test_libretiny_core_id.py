"""LibreTiny gets the xPortGetCoreID stand-in only at framework loglevels that compile log_d in."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

_FLAG = "-DxPortGetCoreID=int"


@pytest.mark.parametrize(
    ("loglevel", "expected"),
    [
        ("verbose", True),
        ("trace", True),
        ("debug", True),
        ("info", False),
        ("warn", False),
        (None, False),
    ],
)
def test_core_id_flag_only_at_debug_loglevels(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    loglevel: str | None,
    expected: bool,
) -> None:
    framework = f"\n  framework:\n    loglevel: {loglevel}" if loglevel else ""
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\n"
        f"bk72xx:\n  board: generic-bk7231n-qfn32-tuya{framework}\n\n"
        "wifi:\n  ssid: test\n  password: testtest12\n\n"
        "web_server:\n"
    )
    generate_main(config)
    assert (_FLAG in CORE.build_flags) is expected
