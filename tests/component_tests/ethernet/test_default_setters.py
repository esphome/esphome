"""Ethernet setters are skipped when the config matches the C++ initializers."""

from collections.abc import Callable
from pathlib import Path

_MDIO_SETTERS = ("set_phy_addr(", "set_mdc_pin(", "set_mdio_pin(")


def test_rmii_defaults_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("rmii_defaults.yaml"))

    for setter in (*_MDIO_SETTERS, "set_clk_mode(", "set_clk_pin("):
        assert setter not in main_cpp


def test_rmii_custom_values_are_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("rmii_custom.yaml"))

    assert "set_phy_addr(1);" in main_cpp
    assert "set_mdc_pin(4);" in main_cpp
    assert "set_mdio_pin(5);" in main_cpp
    assert "set_clk_mode(::EMAC_CLK_OUT);" in main_cpp
    assert "set_clk_pin(17);" in main_cpp


def test_spi2_interface_is_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """ESP32-S3 defaults to spi2, which is the C++ initializer."""
    main_cpp = generate_main(component_config_path("spi_s3_own_bus.yaml"))

    assert "set_interface(" not in main_cpp


def test_spi3_interface_is_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Classic ESP32 defaults to spi3, so the setter is still needed."""
    main_cpp = generate_main(component_config_path("spi_own_bus.yaml"))

    assert "set_interface(::SPI3_HOST);" in main_cpp
