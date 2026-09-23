"""Every preferences platform either emits USE_PREFERENCE_KEY_LOOKUP from
codegen (key-lookup backends) or must not (slot-based backends, whose managers
have no load_from_key()). Run each platform's real codegen and assert the
emission, mirroring the split the deny-list in esphome/core/defines.h assumes
for static analysis.

The fixtures cover every distinct preferences backend today: ln882x and
rtl87xx route through libretiny (bk72xx stands in for the family), rp2040 is
an alias of rp2, and nrf52 exercises zephyr. A seventh backend needs a new
fixture here."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    ("fixture", "emits"),
    [
        ("esp32.yaml", True),
        ("bk72xx.yaml", True),  # libretiny
        ("host.yaml", True),
        ("nrf52.yaml", True),  # zephyr
        ("esp8266.yaml", False),
        ("rp2.yaml", False),
    ],
)
def test_key_lookup_define_matches_the_platform_backend(
    fixture: str,
    emits: bool,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path(fixture))
    defines = {define.name for define in CORE.defines}
    assert ("USE_PREFERENCE_KEY_LOOKUP" in defines) is emits
