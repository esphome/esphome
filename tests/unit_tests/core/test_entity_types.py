"""esphome/core/entity_types.py mirrors esphome/core/entity_types.h."""

from pathlib import Path
import re

from esphome.core import app_lists
from esphome.core.entity_types import DATETIME_SUBTYPES, ENTITY_PLATFORMS

ENTITY_TYPES_H = (
    Path(__file__).parents[3] / "esphome" / "core" / "entity_types.h"
).read_text(encoding="utf-8")
# (singular, upper) of every ENTITY_TYPE_ / ENTITY_CONTROLLER_TYPE_ entry, in header order
ENTRIES = re.findall(
    r"ENTITY_(?:CONTROLLER_)?TYPE_\(\s*\w+::\w+,\s*(\w+),\s*\w+,\s*\w+,\s*(\w+)",
    ENTITY_TYPES_H,
)


def test_platforms_match_entity_types_h() -> None:
    assert tuple(singular for singular, _ in ENTRIES) == ENTITY_PLATFORMS


def test_upper_tokens_match_entity_types_h() -> None:
    """The DATETIME_ prefix of date/time/datetime and every other `upper` token match the header."""
    assert dict(ENTRIES) == app_lists.ENTITY_LISTS
    assert {
        singular for singular, upper in ENTRIES if upper.startswith("DATETIME_")
    } == DATETIME_SUBTYPES
