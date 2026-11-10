"""The code generated for keeloq remotes."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.esp8266.const import KEY_ESP8266, KEY_PREFERENCES
from esphome.core import CORE


def test_each_remote_reserves_its_counter_in_flash(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("esp8266_remotes.yaml"))
    # Two remotes, each with a uint16_t counter: a data word plus the checksum word in the flash store
    assert CORE.data[KEY_ESP8266][KEY_PREFERENCES] == [(2, True)] * 2
