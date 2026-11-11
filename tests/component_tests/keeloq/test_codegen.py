"""The code generated for keeloq remotes and their actions."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp8266.const import KEY_ESP8266, KEY_PREFERENCES
from esphome.core import CORE


def test_each_remote_reserves_what_it_saves_in_flash(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("esp8266_remotes.yaml"))
    # Each value takes its data words plus the checksum word in the flash store.
    # The uint16_t counter: one data word
    counter = (2, True)
    # The last press, counter and a flag in 3 bytes: one data word
    last_press = (2, True)
    # What a remote without serial took over, serial, reserved block and channels in 8 bytes: two data words
    taken_over = (3, True)
    assert CORE.data[KEY_ESP8266][KEY_PREFERENCES] == [
        *(counter, last_press),  # shutter_remote
        *(counter, last_press),  # awning_remote
        *(taken_over, last_press),  # hand_remote, without serial
    ]


@pytest.mark.parametrize(
    "call",
    [
        # shade is stop (0x4) for channels 1 and 2, held: sent 20 times without a pause
        "::shutter_remote->send(::transmitter, 4, 0x03, 20, 0);",
        # a button code is sent twice, like a press of a name
        "::shutter_remote->send(::transmitter, 4, 0x01, 2, 0);",
        # repeat sets the count and the wait in microseconds
        "::shutter_remote->send(::transmitter, 8, 0x01, 3, 10000);",
        "::shutter_remote->learn(::transmitter, 0x01);",
        # without channels on the action, the remote's own as they are when it runs
        "::awning_remote->send(::transmitter, 8, ::awning_remote->get_channels(), 2, 0);",
        "::awning_remote->learn(::transmitter, ::awning_remote->get_channels());",
    ],
    ids=["shade", "code", "repeat", "learn", "send-own-channels", "learn-own-channels"],
)
def test_actions_call_the_remote(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    call: str,
) -> None:
    assert call in generate_main(component_config_path("esp8266_remotes.yaml"))
