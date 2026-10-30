from typing import Any

import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_TEMPERATURE_SENSOR
import esphome.config_validation as cv
from esphome.const import CONF_DATA_RATE
from esphome.cpp_generator import MockObj

from . import MONOCHROME, EpaperModel, ultrachip_resolution

# The gate count Seeed's sample code sets for this 480 row panel
GATES = 680


class SSD2677(EpaperModel):
    source_files = (
        "epaper_spi_ultrachip.cpp",
        "epaper_spi_ssd2677.cpp",
    )

    def __init__(
        self,
        name: str,
        class_name: str = "EPaperSSD2677",
        data_rate: str = "10MHz",
        **defaults: Any,
    ) -> None:
        defaults[CONF_DATA_RATE] = data_rate
        defaults.setdefault("partial_update", True)
        super().__init__(name, class_name=class_name, **defaults)

    def get_config_options(self) -> dict:
        return {
            **super().get_config_options(),
            cv.Optional(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        }

    async def to_code(self, var: MockObj, config: dict) -> dict:
        if (temperature_sensor := config.get(CONF_TEMPERATURE_SENSOR)) is not None:
            cg.add(
                var.set_temperature_sensor(await cg.get_variable(temperature_sensor))
            )
        return config

    # fmt: off
    def get_init_sequence(self, config: dict):
        width, _height = self.get_dimensions(config)
        return (
            (0x00, 0x2F, 0x0E),    # Panel setting
            (0x06, 0x0F, 0x8B, 0x93, 0xC1),    # Booster soft start
            (0xE7, 0xC1),
            (0x30, 0x08),    # PLL
            (0x50, 0x77),    # VCOM and data interval
            (0x62, 0x76, 0x76, 0x76, 0x5A, 0x9D, 0x8A, 0x76, 0x62),    # Timing
            ultrachip_resolution(width, GATES),
            (0xE0, 0x10),
            (0x65, 0x00, 0x00, 0x00, 0x00),    # Gate start
            (0xE9, 0x01),
        )


ssd2677 = SSD2677("ssd2677")

# Sticky version 2 - monochrome
seeed_sticky_v2 = ssd2677.extend(
    "seeed-reterminal-sticky-v2",
    width=800,
    height=480,
    mirror_x=True,
    enable_pin=47,
    cs_pin=15,
    dc_pin=16,
    reset_pin=17,
    busy_pin={
        "number": 18,
        "inverted": True,
    },
    reset_duration="20ms",
    requires={"psram"},
)

# Sticky version 2 - 4 level grayscale; partial updates only in black and white, on request
seeed_sticky_v2.extend(
    "seeed-reterminal-sticky-v2-gray4",
    class_name="EPaperSSD2677Gray4",
    partial_update=MONOCHROME,
    # each plane byte is built from two whole buffer bytes
    width_multiple=8,
)
