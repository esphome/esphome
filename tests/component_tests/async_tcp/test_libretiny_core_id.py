"""LibreTiny always gets the xPortGetCoreID stand-in, whatever its log level."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_core_id_flag_at_default_loglevel(
    generate_main: Callable[[str | Path], str], tmp_path: Path
) -> None:
    # CONFIG_ASYNC_TCP_DEBUG compiles the call in at LibreTiny's default warn level
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\n"
        "bk72xx:\n  board: generic-bk7231n-qfn32-tuya\n\n"
        "wifi:\n  ssid: test\n  password: testtest12\n\n"
        "web_server:\n"
    )
    generate_main(config)
    assert "-DxPortGetCoreID=int" in CORE.build_flags
