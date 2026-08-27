"""The host pch prefix must keep resolving; a rename would silently
collapse the precompiled set to defines.h with strict CI still green."""

from pathlib import Path
import re

from esphome.components.host import HOST_PCH_PREFIX

REPO = Path(__file__).parents[2]


def test_host_pch_prefix_resolves() -> None:
    prefix = REPO / HOST_PCH_PREFIX
    assert prefix.is_file()
    body = prefix.read_text()
    includes = re.findall(r'#include "([^"]+)"', body)
    assert includes, "prefix wrapper folds nothing"
    for name in includes:
        assert (REPO / name).is_file(), f"{name} does not resolve"
    # The C guard is what keeps build_src_flags safe on C/assembly edges
    assert "#ifdef __cplusplus" in body
