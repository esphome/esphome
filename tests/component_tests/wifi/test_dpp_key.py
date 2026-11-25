"""The DPP bootstrapping key is given as 32 raw bytes in hex and handed to the supplicant DER encoded."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import wifi
import esphome.config_validation as cv

RAW_KEY = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"


@pytest.mark.parametrize(
    "value",
    [
        RAW_KEY[:-2],
        RAW_KEY + "00",
        "zz" + RAW_KEY[2:],
        wifi.DPP_KEY_DER_PREFIX + RAW_KEY + wifi.DPP_KEY_DER_SUFFIX,
    ],
)
def test_key_must_be_64_hex_digits(value: str) -> None:
    with pytest.raises(cv.Invalid, match="64 hexadecimal characters"):
        wifi._validate_dpp_private_key(value)


def test_key_is_lower_cased() -> None:
    assert wifi._validate_dpp_private_key(RAW_KEY.upper()) == RAW_KEY


def test_generated_key_is_der_encoded(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The ESP-IDF supplicant expects the P-256 DER header around the raw key."""
    main_cpp = generate_main(component_config_path("dpp_key.yaml"))

    assert (
        f'set_dpp_key("{wifi.DPP_KEY_DER_PREFIX}{RAW_KEY}{wifi.DPP_KEY_DER_SUFFIX}")'
        in main_cpp
    )
