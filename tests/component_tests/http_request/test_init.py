"""Tests for the http_request watchdog timeout default."""

from collections.abc import Callable
from pathlib import Path

import pytest

YAML_DIR = Path(__file__).parent


@pytest.mark.parametrize(
    ("yaml_file", "expected_ms"),
    [
        # 3 x 10s plus 1s margin
        ("test_esp32_default.yaml", 31000),
        # esp32.watchdog_timeout: 60s is wider than the derived value and wins
        ("test_esp32_platform_wider.yaml", 60000),
        # explicit value is kept as is
        ("test_esp32_explicit.yaml", 20000),
    ],
)
def test_esp32_watchdog_timeout(
    generate_main: Callable[[str | Path], str], yaml_file: str, expected_ms: int
) -> None:
    main_cpp = generate_main(YAML_DIR / yaml_file)
    assert f"set_watchdog_timeout({expected_ms})" in main_cpp


def test_esp8266_leaves_watchdog_unset(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(YAML_DIR / "test_esp8266.yaml")
    assert "set_watchdog_timeout" not in main_cpp
