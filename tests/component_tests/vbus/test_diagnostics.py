"""RX diagnostics are optional and do not require a transmitter."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize("enabled", [None, False, True])
def test_rx_diagnostics_are_compile_time_optional_without_tx(
    enabled: bool | None,
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
    generate_main: Callable[[str | Path], str],
) -> None:
    source = component_config_path("rx_only.yaml").read_text()
    if enabled is not None:
        source = source.replace(
            "vbus:\n", f"vbus:\n  rx_diagnostics: {str(enabled).lower()}\n"
        )
    path = tmp_path / "diagnostics.yaml"
    path.write_text(source)
    generate_main(path)
    defines = {define.name for define in CORE.defines}
    assert ("USE_VBUS_RX_DIAGNOSTICS" in defines) is (enabled is True)
    assert "USE_VBUS_TX" not in defines
