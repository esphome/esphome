from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_PIN

from . import EpaperModel

CONF_CS1_PIN = "cs1_pin"


class T133A01(EpaperModel):
    def __init__(
        self,
        name: str,
        class_name: str = "EPaperT133A01",
        initsequence=(),
        **defaults,
    ):
        super().__init__(name, class_name, initsequence=tuple(initsequence), **defaults)

    def get_config_schema(self) -> dict:
        return {
            cv.Optional(
                CONF_CS1_PIN,
                default=self.get_default(CONF_CS1_PIN, cv.UNDEFINED),
            ): pins.gpio_output_pin_schema,
            self.option(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
        }

    async def to_code(self, config: dict, var) -> None:
        if cs1_pin := config.get(CONF_CS1_PIN):
            cs1 = await cg.gpio_pin_expression(cs1_pin)
            cg.add(var.set_cs1_pin(cs1))
        if enable_pin := config.get(CONF_ENABLE_PIN):
            enable = await cg.gpio_pin_expression(enable_pin)
            cg.add(var.set_enable_pin(enable))

    # fmt: off
    def get_init_sequence(self, config: dict):
        return (
            (0x74, 0xC0, 0x1C, 0x1C, 0xCC, 0xCC, 0xCC, 0x15, 0x15, 0x55),
            (0xF0, 0x49, 0x55, 0x13, 0x5D, 0x05, 0x10),
            (0x00, 0xDF, 0x69),
            (0x50, 0x37),
            (0x60, 0x03, 0x03),
            (0x86, 0x10),
            (0xE3, 0x22),
            (0x61, 0x04, 0xB0, 0x03, 0x20),
            (0x01, 0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38),
            (0xB6, 0x07),
            (0x06, 0xD8, 0x18),
            (0xB7, 0x01),
            (0x05, 0xD8, 0x18),
            (0xB0, 0x01),
            (0xB1, 0x02),
        )


t133a01 = T133A01(
    "T133A01",
    width=1200,
    height=1600,
    data_rate="10MHz",
    minimum_update_interval="30s",
    reset_duration="20ms",
)


t133a01.extend(
    "SEEED-EE02-COLOR-13.3",
    cs_pin=44,
    cs1_pin=41,
    dc_pin=10,
    busy_pin={
        "number": 4,
        "inverted": True,
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
    reset_pin=38,
    enable_pin=43,
)
