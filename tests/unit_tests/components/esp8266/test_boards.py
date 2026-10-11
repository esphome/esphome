"""Tests for the per-board linker-script rule."""

from esphome.components.esp8266 import _choose_ld_script
from esphome.components.esp8266.boards import BOARDS, board_ld_script


def test_d1_wroom_02_keeps_its_shipped_layout() -> None:
    """The override must survive a BOARDS regeneration or key typo: the
    2m.ld default moves _FS_end and the preferences sector on deployed
    devices."""
    assert board_ld_script(BOARDS["d1_wroom_02"]) == "eagle.flash.2m64.ld"


def test_default_boards_use_the_flash_size_layout() -> None:
    assert board_ld_script(BOARDS["d1_mini"]) == "eagle.flash.4m.ld"
    assert board_ld_script(BOARDS["esp01_1m"]) == "eagle.flash.1m.ld"


def test_choose_ld_script_paths() -> None:
    """Default boards get the size layout, overriding boards keep theirs."""
    assert _choose_ld_script("nodemcuv2") == "eagle.flash.4m.ld"
    assert _choose_ld_script("d1_wroom_02") == "eagle.flash.2m64.ld"
