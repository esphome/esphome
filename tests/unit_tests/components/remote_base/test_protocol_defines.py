"""Every gated protocol source needs its define listed for static analysis."""

from pathlib import Path

from esphome.components.remote_base import _PROTOCOL_STEMS, protocol_define

DEFINES_H = Path(__file__).parents[4] / "esphome" / "core" / "defines.h"


def test_every_protocol_define_is_listed_in_defines_h() -> None:
    """A protocol compiled only behind its define would drop out of clang-tidy without this line."""
    defines = DEFINES_H.read_text(encoding="utf-8")
    missing = [
        stem
        for stem in _PROTOCOL_STEMS
        if f"#define {protocol_define(stem)}\n" not in defines
    ]
    assert not missing, (
        f"Add #define {', '.join(protocol_define(s) for s in missing)} to {DEFINES_H.name}"
    )
