# BS0/BS1 select the controllers' interface mode. The driver's reset() switches the reset and
# enable pins itself on every refresh cycle, so reset_duration is rejected.

from collections.abc import Hashable
from typing import Any

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DC_PIN,
    CONF_ENABLE_PIN,
    CONF_INVERTED,
    CONF_RESET_DURATION,
    CONF_RESET_PIN,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from .dualcs import BOTH, CMD66_DATA, CONF_CS1_PIN, PRIMARY, DualCSModel

CONF_BS0_PIN = "bs0_pin"
CONF_BS1_PIN = "bs1_pin"


def _check_inverted_pin_is_internal(pin: ConfigType, path: list[Hashable]) -> None:
    if (
        pin.get(CONF_INVERTED)
        and pins.PIN_SCHEMA_REGISTRY.get_key(pin) != CORE.target_platform
    ):
        raise cv.Invalid(
            "Only a pin on the chip itself can be inverted for this model", path=path
        )


class Inkplate13SpectraModel(DualCSModel):
    source_files = DualCSModel.source_files + ("epaper_spi_inkplate13spectra.cpp",)

    def __init__(
        self, name: str, class_name: str = "EPaperInkplate13Spectra", **defaults: Any
    ) -> None:
        super().__init__(name, class_name, **defaults)

    def get_init_sequence(self, config: ConfigType) -> tuple[tuple[int, ...], ...]:
        # The register values of Soldered's own driver (Inkplate13SPECTRADriver.cpp, screenInit())
        return (
            (
                0x74,
                PRIMARY,
                0xC0,
                0x1C,
                0x1C,
                0xCC,
                0xCC,
                0xCC,
                0x15,
                0x15,
                0x55,
            ),  # AN_TM
            (0xF0, BOTH, *CMD66_DATA),
            (0x00, BOTH, 0xDF, 0x6B),  # PSR
            (0x30, BOTH, 0x08),  # PLL
            (0x50, BOTH, 0xF7),  # CDI
            (0x60, BOTH, 0x03, 0x03),  # TCON
            (0x86, BOTH, 0x10),  # AGID
            (0xE3, BOTH, 0x22),  # PWS
            (0xE0, BOTH, 0x01),  # CCSET
            self.resolution(config),  # TRES
            (0x01, PRIMARY, 0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38),  # PWR
            (0xB6, PRIMARY, 0x07),  # EN_BUF
            (0x06, PRIMARY, 0xD8, 0x18),  # BTST_P
            (0xB7, PRIMARY, 0x01),  # BOOST_VDDP_EN
            (0x05, PRIMARY, 0xD8, 0x18),  # BTST_N
            (0xB0, PRIMARY, 0x01),  # BUCK_BOOST_VDDN
            (0xB1, PRIMARY, 0x02),  # TFT_VCOM_POWER
        )

    def validate_config(self, config: ConfigType) -> ConfigType:
        config = super().validate_config(config)
        if CONF_RESET_DURATION in config:
            raise cv.Invalid(
                f"{self.name} sets its own reset timing; remove '{CONF_RESET_DURATION}'",
                path=[CONF_RESET_DURATION],
            )
        # The driver drives every panel pin physically low to drain the panel. It can only
        # undo the inversion of a pin on the chip itself, not on an I/O expander.
        for key in (
            CONF_BUSY_PIN,
            CONF_CS_PIN,
            CONF_CS1_PIN,
            CONF_DC_PIN,
            CONF_RESET_PIN,
            CONF_BS0_PIN,
            CONF_BS1_PIN,
        ):
            _check_inverted_pin_is_internal(config[key], [key])
        for index, pin in enumerate(config[CONF_ENABLE_PIN]):
            _check_inverted_pin_is_internal(pin, [CONF_ENABLE_PIN, index])
        return config

    def get_config_options(self) -> dict[Any, Any]:
        return {
            **super().get_config_options(),
            self.option(CONF_BS0_PIN, fallback=None): pins.gpio_output_pin_schema,
            self.option(CONF_BS1_PIN, fallback=None): pins.gpio_output_pin_schema,
        }

    async def to_code(self, var: MockObj, config: ConfigType) -> ConfigType:
        config = await super().to_code(var, config)
        await cg.add_pin(var.set_bs0_pin, config[CONF_BS0_PIN])
        await cg.add_pin(var.set_bs1_pin, config[CONF_BS1_PIN])
        return {
            k: v for k, v in config.items() if k not in (CONF_BS0_PIN, CONF_BS1_PIN)
        }


# Confirmed on real hardware: rotation=0 needs mirror_x + mirror_y but not swap_xy.
inkplate13spectra = Inkplate13SpectraModel(
    "inkplate13spectra",
    width=1200,
    height=1600,
    swap_xy=False,
    mirror_x=True,
    mirror_y=True,
    data_rate="10MHz",
    partial_update=True,
    # A full 6-color refresh on a panel this large takes tens of seconds; disallow
    # faster updates to avoid FSM update loops.
    minimum_update_interval="30s",
    # Default GPIO pins for the on-board Inkplate 13 Spectra wiring.
    dc_pin=14,
    cs_pin=42,
    cs1_pin=39,
    bs0_pin=6,
    bs1_pin=5,
    reset_pin=4,
    enable_pin=21,
    busy_pin={
        "number": 7,
        "inverted": True,  # hardware: LOW=busy, HIGH=idle
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
)
