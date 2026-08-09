"""Pin the USE_PREFERENCE_KEY_LOOKUP deny-list in esphome/core/defines.h to the
platforms whose codegen actually emits the define, so neither side can drift
from the other unnoticed. Follows the defines.h mirror precedent in
tests/component_tests/bluetooth_proxy/test_platform_gates.py."""

from pathlib import Path
import re

REPO_ROOT = Path(__file__).parents[3]

# Every platform component that provides a preferences manager (the include
# ladder in esphome/core/preferences.h). A new platform must be added here,
# which forces a decision: emit the define (key-lookup) or join the deny-list
# (slot-based).
PREFERENCES_PLATFORMS = {
    "esp32",
    "esp8266",
    "rp2",
    "libretiny",
    "host",
    "zephyr",
}


def _platforms_emitting_key_lookup() -> set[str]:
    return {
        platform
        for platform in PREFERENCES_PLATFORMS
        if 'cg.add_define("USE_PREFERENCE_KEY_LOOKUP")'
        in (REPO_ROOT / "esphome" / "components" / platform / "__init__.py").read_text()
    }


def test_defines_h_deny_list_mirrors_the_codegen_emitters() -> None:
    defines = (REPO_ROOT / "esphome" / "core" / "defines.h").read_text()
    match = re.search(
        r"#if ((?:!defined\(USE_[A-Z0-9]+\)(?: && )?)+)\s*\n#define USE_PREFERENCE_KEY_LOOKUP\b",
        defines,
    )
    assert match is not None, (
        "defines.h no longer guards USE_PREFERENCE_KEY_LOOKUP with a deny-list"
    )
    denied = {
        name.removeprefix("USE_").lower()
        for name in re.findall(r"!defined\((USE_[A-Z0-9]+)\)", match.group(1))
    }

    slot_based = PREFERENCES_PLATFORMS - _platforms_emitting_key_lookup()
    assert denied == slot_based, (
        f"defines.h denies {sorted(denied)} but the platforms without a "
        f'cg.add_define("USE_PREFERENCE_KEY_LOOKUP") emitter are {sorted(slot_based)}'
    )


def test_platform_list_is_current() -> None:
    # If a platform gains or loses a preferences manager, PREFERENCES_PLATFORMS
    # must follow the include ladder in esphome/core/preferences.h.
    ladder = (REPO_ROOT / "esphome" / "core" / "preferences.h").read_text()
    in_ladder = {
        path.split("/")[2]
        for path in re.findall(
            r'#include "(esphome/components/[a-z0-9_]+/preferences\.h)"', ladder
        )
    }
    assert in_ladder == PREFERENCES_PLATFORMS
