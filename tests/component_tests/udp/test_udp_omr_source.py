"""Tests for when the udp component binds its IPv6 send socket to the OMR address."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

CONFIG = """
esphome:
  name: test

nrf52:
  board: adafruit_itsybitsy_nrf52840
  bootloader: adafruit_nrf52_sd140_v6

openthread:
  tlv: 0E080000000000010000

network:
  enable_ipv6: true

udp:
  addresses: [{addresses}]
"""


@pytest.mark.parametrize(
    ("addresses", "expected"),
    [
        pytest.param(["2001:db8::1"], True, id="global_unicast"),
        pytest.param(["ff05::1"], True, id="site_multicast"),
        pytest.param(["ff03::1", "ff02::1"], False, id="mesh_multicast"),
        pytest.param(["fd11:22::1"], False, id="unique_local"),
        pytest.param(["ff03::1", "fd11:22::1", "2001:db8::1"], True, id="mixed"),
    ],
)
def test_omr_source_only_for_off_mesh_destinations(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    addresses: list[str],
    expected: bool,
) -> None:
    """Only a destination outside the Thread mesh needs the routable source bind."""
    config = tmp_path / "udp_omr.yaml"
    config.write_text(CONFIG.format(addresses=", ".join(f'"{a}"' for a in addresses)))
    main_cpp = generate_main(config)

    assert any(d.name == "USE_UDP_OMR_SOURCE" for d in CORE.defines) is expected
    assert ("set_omr_source()" in main_cpp) is expected


TWO_INSTANCES = """
esphome:
  name: test

nrf52:
  board: adafruit_itsybitsy_nrf52840
  bootloader: adafruit_nrf52_sd140_v6

openthread:
  tlv: 0E080000000000010000

network:
  enable_ipv6: true

udp:
  - id: udp_mesh
    port: 18511
    addresses: ["ff03::1", "fd11:22::1"]
  - id: udp_global
    port: 18512
    addresses: ["2001:db8::1"]
"""


def test_omr_source_setter_only_for_instances_that_need_it(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    """Only the instance with an off-mesh destination waits for the OMR source bind."""
    config = tmp_path / "udp_omr_two.yaml"
    config.write_text(TWO_INSTANCES)
    main_cpp = generate_main(config)

    assert "udp_global->set_omr_source();" in main_cpp
    assert "udp_mesh->set_omr_source();" not in main_cpp
    assert main_cpp.count("set_omr_source()") == 1
