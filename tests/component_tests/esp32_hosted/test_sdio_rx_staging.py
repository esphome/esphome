"""Tests for the wrap that keeps the SDIO RX staging buffers growing on demand."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

_WRAP_FLAGS = {
    "-Wl,--wrap=eh_host_bus_sdio_set_rx_staging_target",
    "-Wl,--undefined=__wrap_eh_host_bus_sdio_set_rx_staging_target",
}

_SDIO = (
    "  cmd_pin: GPIO19\n  clk_pin: GPIO18\n  d0_pin: GPIO14\n  d1_pin: GPIO15\n"
    "  d2_pin: GPIO16\n  d3_pin: GPIO17\n"
)
_SPI = (
    "  type: spi\n  handshake_pin: GPIO17\n  data_ready_pin: GPIO14\n"
    "  miso_pin: GPIO10\n  mosi_pin: GPIO11\n  clk_pin: GPIO9\n  cs_pin: GPIO53\n"
)


@pytest.mark.parametrize(
    ("bus", "expected"),
    [
        pytest.param(_SDIO, True, id="sdio"),
        pytest.param(_SPI, False, id="spi"),
    ],
)
def test_wrap_only_on_sdio(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    bus: str,
    expected: bool,
) -> None:
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\nesp32:\n  variant: esp32p4\n"
        "  framework: {type: esp-idf}\n\n"
        f"esp32_hosted:\n  variant: ESP32C6\n  reset_pin: GPIO54\n{bus}"
    )
    generate_main(config)
    assert CORE.build_flags & _WRAP_FLAGS == (_WRAP_FLAGS if expected else set())
