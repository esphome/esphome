"""The select options line up with the command tables they index."""

from pathlib import Path
import re

import pytest

from esphome.components.ld6004.select import P20_MODE_OPTIONS, WORK_MODE_OPTIONS

SOURCE = (
    Path(__file__).parents[3] / "esphome" / "components" / "ld6004" / "ld6004.cpp"
).read_text()


def _table(name: str) -> list[str]:
    match = re.search(rf"{name}\[\w+\] PROGMEM = \{{(.*?)\}};", SOURCE, re.DOTALL)
    assert match is not None, name
    return [entry.strip() for entry in match.group(1).split(",")]


@pytest.mark.parametrize(
    ("options", "table", "prefix"),
    [
        (WORK_MODE_OPTIONS, "WORK_MODE_COMMANDS", "CMD_WORK_"),
        (P20_MODE_OPTIONS, "P20_MODE_COMMANDS", "CMD_P20_"),
    ],
)
def test_options_match_commands(
    options: tuple[str, ...], table: str, prefix: str
) -> None:
    assert _table(table) == [prefix + option.upper() for option in options]


def test_work_mode_text_matches_select() -> None:
    match = re.search(
        r"PROGMEM_STRING_TABLE\(WorkModeStrings, (.*?)\);", SOURCE, re.DOTALL
    )
    assert match is not None
    names = [name.strip().strip('"') for name in match.group(1).split(",")]
    assert names == list(WORK_MODE_OPTIONS)
