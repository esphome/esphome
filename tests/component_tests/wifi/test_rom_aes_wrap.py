"""Tests for the ESP8266 ROM AES key expansion wrap emitted by the wifi codegen."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

WRAP_FLAG = "-Wl,--wrap=rijndaelKeySetupEnc"


@pytest.mark.parametrize(
    ("config_file", "wrapped"),
    [
        ("bare.yaml", True),
        ("ap_only.yaml", True),
        ("three_networks.yaml", True),
        # The Enterprise TLS client needs AES-256, which the ROM cannot expand
        ("eap.yaml", False),
        ("eap_mixed.yaml", False),
        ("bare_ln882x.yaml", False),
    ],
)
def test_rom_aes_wrap_flag(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    wrapped: bool,
) -> None:
    """ESP8266 builds without EAP wrap the SDK key expansion; EAP and other platforms do not."""
    generate_main(component_config_path(config_file))
    assert (WRAP_FLAG in CORE.build_flags) is wrapped
