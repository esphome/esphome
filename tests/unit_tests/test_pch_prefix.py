"""The pch prefix must keep resolving; a rename would silently
collapse the precompiled set to defines.h with strict CI still green."""

from pathlib import Path
import re

from esphome.build_helpers.pch import PCH_PREFIX_HEADER

REPO = Path(__file__).parents[2]


def test_pch_prefix_resolves() -> None:
    prefix = REPO / PCH_PREFIX_HEADER
    assert prefix.is_file()
    body = prefix.read_text()
    includes = re.findall(r'#include "([^"]+)"', body)
    assert includes, "prefix wrapper folds nothing"
    for name in includes:
        assert (REPO / name).is_file(), f"{name} does not resolve"
    # The C guard is what keeps build_src_flags safe on C/assembly edges
    assert "#ifdef __cplusplus" in body
