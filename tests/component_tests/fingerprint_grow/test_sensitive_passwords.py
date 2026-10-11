"""Test numeric password masking through validation and code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import yaml_util
from esphome.core import CORE


@pytest.mark.parametrize(
    ("platform", "tx_pin", "rx_pin"),
    [
        ("esp32:\n  board: esp32dev\n  framework:\n    type: esp-idf", 1, 3),
        ("esp8266:\n  board: d1_mini", 1, 3),
        ("rp2040:\n  board: rpipico", 0, 1),
    ],
)
def test_passwords_remain_numeric_and_are_masked(
    tmp_path: Path,
    generate_main: Callable[[Path], str],
    platform: str,
    tx_pin: int,
    rx_pin: int,
) -> None:
    """Conceal both password fields without changing generated setter values."""
    path = tmp_path / "fingerprint.yaml"
    path.write_text(
        f"""esphome:
  name: numeric-password-test
{platform}
uart:
  tx_pin: {tx_pin}
  rx_pin: {rx_pin}
  baud_rate: 57600
fingerprint_grow:
  password: 305419896
  new_password: 591751049
""",
        encoding="utf-8",
    )
    cpp = generate_main(path)
    component = CORE.config["fingerprint_grow"][0]
    assert component["password"] == 305419896
    assert component["new_password"] == 591751049
    assert "set_password(305419896)" in cpp
    assert "set_new_password(591751049)" in cpp
    masked = yaml_util.dump(component)
    assert "\\033[8m305419896\\033[28m" in masked
    assert "\\033[8m591751049\\033[28m" in masked
