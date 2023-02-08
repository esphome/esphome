from dataclasses import dataclass
from typing import Any
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_METHOD,
    CONF_PIN,
    CONF_SPEED,
)
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32S2,
    VARIANT_ESP32C3,
)
from esphome.core import CORE
from .const import (
    CONF_ASYNC,
    CONF_BUS,
    CHIP_400KBPS,
    CHIP_800KBPS,
    CHIP_APA106,
    CHIP_DOTSTAR,
    CHIP_LC8812,
    CHIP_LPD6803,
    CHIP_LPD8806,
    CHIP_P9813,
    CHIP_SK6812,
    CHIP_TM1814,
    CHIP_TM1829,
    CHIP_TM1914,
    CHIP_WS2801,
    CHIP_WS2811,
    CHIP_WS2812,
    CHIP_WS2812X,
    CHIP_WS2813,
    ONE_WIRE_CHIPS,
    TWO_WIRE_CHIPS,
)

METHOD_BIT_BANG = "bit_bang"
METHOD_ESP8266_UART = "esp8266_uart"
METHOD_ESP8266_DMA = "esp8266_dma"
METHOD_ESP32_RMT = "esp32_rmt"
METHOD_ESP32_I2S = "esp32_i2s"
METHOD_SPI = "spi"

CHANNEL_DYNAMIC = "dynamic"
BUS_DYNAMIC = "dynamic"
SPI_BUS_VSPI = "vspi"
SPI_BUS_HSPI = "hspi"
SPI_SPEEDS = [40e6, 20e6, 10e6, 5e6, 2e6, 1e6, 500e3]


def _esp32_rmt_default_channel():
    return {
        VARIANT_ESP32S2: 1,
        VARIANT_ESP32C3: 1,
    }.get(get_esp32_variant(), 6)


def _validate_esp32_rmt_channel(value):
    if isinstance(value, str) and value.lower() == CHANNEL_DYNAMIC:
        value = CHANNEL_DYNAMIC
    else:
        value = cv.int_(value)
    variant_channels = {
        VARIANT_ESP32: [0, 1, 2, 3, 4, 5, 6, 7, CHANNEL_DYNAMIC],
        VARIANT_ESP32S2: [0, 1, 2, 3, CHANNEL_DYNAMIC],
        VARIANT_ESP32C3: [0, 1, CHANNEL_DYNAMIC],
    }
    variant = get_esp32_variant()
    if variant not in variant_channels:
        raise cv.Invalid(f"{variant} does not support the rmt method")
    if value not in variant_channels[variant]:
        raise cv.Invalid(f"{variant} does not support rmt channel {value}")
    return value


def _esp32_i2s_default_bus():
    return {
        VARIANT_ESP32: 1,
        VARIANT_ESP32S2: 0,
    }.get(get_esp32_variant(), 0)


def _validate_esp32_i2s_bus(value):
    if isinstance(value, str) and value.lower() == BUS_DYNAMIC:
        value = BUS_DYNAMIC
    else:
        value = cv.int_(value)
    variant_buses = {
        VARIANT_ESP32: [0, 1, BUS_DYNAMIC],
        VARIANT_ESP32S2: [0, BUS_DYNAMIC],
    }
    variant = get_esp32_variant()
    if variant not in variant_buses:
        raise cv.Invalid(f"{variant} does not support the i2s method")
    if value not in variant_buses[variant]:
        raise cv.Invalid(f"{variant} does not support i2s bus {value}")
    return value


neo_ns = cg.global_ns


def _bit_bang_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEspBitBangMethod.h
    # Some chips are only aliases
    chip = {
        CHIP_WS2813: CHIP_WS2812X,
        CHIP_LC8812: CHIP_SK6812,
        CHIP_TM1914: CHIP_TM1814,
        CHIP_WS2812: CHIP_800KBPS,
    }.get(chip, chip)

    lookup = {
        CHIP_WS2811: (neo_ns.NeoEspBitBangSpeedWs2811, False),
        CHIP_WS2812X: (neo_ns.NeoEspBitBangSpeedWs2812x, False),
        CHIP_SK6812: (neo_ns.NeoEspBitBangSpeedSk6812, False),
        CHIP_TM1814: (neo_ns.NeoEspBitBangSpeedTm1814, True),
        CHIP_TM1829: (neo_ns.NeoEspBitBangSpeedTm1829, True),
        CHIP_800KBPS: (neo_ns.NeoEspBitBangSpeed800Kbps, False),
        CHIP_400KBPS: (neo_ns.NeoEspBitBangSpeed400Kbps, False),
        CHIP_APA106: (neo_ns.NeoEspBitBangSpeedApa106, False),
    }
    # For tm variants opposite of inverted is needed
    speed, pinset_inverted = lookup[chip]
    pinset = {
        False: neo_ns.NeoEspPinset,
        True: neo_ns.NeoEspPinsetInverted,
    }[inverted != pinset_inverted]
    return neo_ns.NeoEspBitBangMethodBase.template(speed, pinset)


def _bit_bang_extra_validate(config):
    pin = config[CONF_PIN]
    if CORE.is_esp8266 and not (0 <= pin <= 15):
        # Due to use of w1ts
        raise cv.Invalid("Bit bang only supports pins GPIO0-GPIO15 on ESP8266")
    if CORE.is_esp32 and not (0 <= pin <= 31):
        raise cv.Invalid("Bit bang only supports pins GPIO0-GPIO31 on ESP32")


def _esp8266_uart_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEsp8266UartMethod.h
    uart_context, uart_base = {
        False: (neo_ns.NeoEsp8266UartContext, neo_ns.NeoEsp8266Uart),
        True: (neo_ns.NeoEsp8266UartInterruptContext, neo_ns.NeoEsp8266AsyncUart),
    }[config[CONF_ASYNC]]
    uart_feature = {
        0: neo_ns.UartFeature0,
        1: neo_ns.UartFeature1,
    }[config[CONF_BUS]]
    # Some chips are only aliases
    chip = {
        CHIP_WS2811: CHIP_WS2812X,
        CHIP_WS2813: CHIP_WS2812X,
        CHIP_LC8812: CHIP_SK6812,
        CHIP_TM1914: CHIP_TM1814,
        CHIP_WS2812: CHIP_800KBPS,
    }.get(chip, chip)

    lookup = {
        CHIP_WS2812X: (neo_ns.NeoEsp8266UartSpeedWs2812x, False),
        CHIP_SK6812: (neo_ns.NeoEsp8266UartSpeedSk6812, False),
        CHIP_TM1814: (neo_ns.NeoEsp8266UartSpeedTm1814, True),
        CHIP_TM1829: (neo_ns.NeoEsp8266UartSpeedTm1829, True),
        CHIP_800KBPS: (neo_ns.NeoEsp8266UartSpeed800Kbps, False),
        CHIP_400KBPS: (neo_ns.NeoEsp8266UartSpeed400Kbps, False),
        CHIP_APA106: (neo_ns.NeoEsp8266UartSpeedApa106, False),
    }
    speed, uart_inverted = lookup[chip]
    # For tm variants opposite of inverted is needed
    inv = {
        False: neo_ns.NeoEsp8266UartNotInverted,
        True: neo_ns.NeoEsp8266UartInverted,
    }[inverted != uart_inverted]
    return neo_ns.NeoEsp8266UartMethodBase.template(
        speed, uart_base.template(uart_feature, uart_context), inv
    )


def _esp8266_uart_extra_validate(config):
    pin = config[CONF_PIN]
    bus = config[CONF_METHOD][CONF_BUS]
    right_pin = {
        0: 1,  # U0TXD
        1: 2,  # U1TXD
    }[bus]
    if pin != right_pin:
        raise cv.Invalid(f"ESP8266 uart bus {bus} only supports pin GPIO{right_pin}")


def _esp8266_dma_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEsp8266DmaMethod.h
    # Some chips are only aliases
    chip = {
        CHIP_WS2811: CHIP_WS2812X,
        CHIP_WS2813: CHIP_WS2812X,
        CHIP_LC8812: CHIP_SK6812,
        CHIP_TM1914: CHIP_TM1814,
        CHIP_WS2812: CHIP_800KBPS,
    }.get(chip, chip)

    lookup = {
        (CHIP_WS2812X, False): neo_ns.NeoEsp8266DmaSpeedWs2812x,
        (CHIP_SK6812, False): neo_ns.NeoEsp8266DmaSpeedSk6812,
        (CHIP_TM1814, True): neo_ns.NeoEsp8266DmaInvertedSpeedTm1814,
        (CHIP_TM1829, True): neo_ns.NeoEsp8266DmaInvertedSpeedTm1829,
        (CHIP_800KBPS, False): neo_ns.NeoEsp8266DmaSpeed800Kbps,
        (CHIP_400KBPS, False): neo_ns.NeoEsp8266DmaSpeed400Kbps,
        (CHIP_APA106, False): neo_ns.NeoEsp8266DmaSpeedApa106,
        (CHIP_WS2812X, True): neo_ns.NeoEsp8266DmaInvertedSpeedWs2812x,
        (CHIP_SK6812, True): neo_ns.NeoEsp8266DmaInvertedSpeedSk6812,
        (CHIP_TM1814, False): neo_ns.NeoEsp8266DmaSpeedTm1814,
        (CHIP_TM1829, False): neo_ns.NeoEsp8266DmaSpeedTm1829,
        (CHIP_800KBPS, True): neo_ns.NeoEsp8266DmaInvertedSpeed800Kbps,
        (CHIP_400KBPS, True): neo_ns.NeoEsp8266DmaInvertedSpeed400Kbps,
        (CHIP_APA106, True): neo_ns.NeoEsp8266DmaInvertedSpeedApa106,
    }
    speed = lookup[(chip, inverted)]
    return neo_ns.NeoEsp8266DmaMethodBase.template(speed)


def _esp8266_dma_extra_validate(config):
    if config[CONF_PIN] != 3:
        raise cv.Invalid("ESP8266 dma method only supports pin GPIO3")


def _esp32_rmt_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEsp32RmtMethod.h
    channel = {
        0: neo_ns.NeoEsp32RmtChannel0,
        1: neo_ns.NeoEsp32RmtChannel1,
        2: neo_ns.NeoEsp32RmtChannel2,
        3: neo_ns.NeoEsp32RmtChannel3,
        4: neo_ns.NeoEsp32RmtChannel4,
        5: neo_ns.NeoEsp32RmtChannel5,
        6: neo_ns.NeoEsp32RmtChannel6,
        7: neo_ns.NeoEsp32RmtChannel7,
        CHANNEL_DYNAMIC: neo_ns.NeoEsp32RmtChannelN,
    }[config[CONF_CHANNEL]]
    # Some chips are only aliases
    chip = {
        CHIP_WS2813: CHIP_WS2812X,
        CHIP_LC8812: CHIP_SK6812,
        CHIP_WS2812: CHIP_800KBPS,
    }.get(chip, chip)

    lookup = {
        (CHIP_WS2811, False): neo_ns.NeoEsp32RmtSpeedWs2811,
        (CHIP_WS2812X, False): neo_ns.NeoEsp32RmtSpeedWs2812x,
        (CHIP_SK6812, False): neo_ns.NeoEsp32RmtSpeedSk6812,
        (CHIP_TM1814, False): neo_ns.NeoEsp32RmtSpeedTm1814,
        (CHIP_TM1829, False): neo_ns.NeoEsp32RmtSpeedTm1829,
        (CHIP_TM1914, False): neo_ns.NeoEsp32RmtSpeedTm1914,
        (CHIP_800KBPS, False): neo_ns.NeoEsp32RmtSpeed800Kbps,
        (CHIP_400KBPS, False): neo_ns.NeoEsp32RmtSpeed400Kbps,
        (CHIP_APA106, False): neo_ns.NeoEsp32RmtSpeedApa106,
        (CHIP_WS2811, True): neo_ns.NeoEsp32RmtInvertedSpeedWs2811,
        (CHIP_WS2812X, True): neo_ns.NeoEsp32RmtInvertedSpeedWs2812x,
        (CHIP_SK6812, True): neo_ns.NeoEsp32RmtInvertedSpeedSk6812,
        (CHIP_TM1814, True): neo_ns.NeoEsp32RmtInvertedSpeedTm1814,
        (CHIP_TM1829, True): neo_ns.NeoEsp32RmtInvertedSpeedTm1829,
        (CHIP_TM1914, True): neo_ns.NeoEsp32RmtInvertedSpeedTm1914,
        (CHIP_800KBPS, True): neo_ns.NeoEsp32RmtInvertedSpeed800Kbps,
        (CHIP_400KBPS, True): neo_ns.NeoEsp32RmtInvertedSpeed400Kbps,
        (CHIP_APA106, True): neo_ns.NeoEsp32RmtInvertedSpeedApa106,
    }
    speed = lookup[(chip, inverted)]
    return neo_ns.NeoEsp32RmtMethodBase.template(speed, channel)


def _esp32_i2s_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/NeoEsp32I2sMethod.h
    bus = {
        0: neo_ns.NeoEsp32I2sBusZero,
        1: neo_ns.NeoEsp32I2sBusOne,
        BUS_DYNAMIC: neo_ns.NeoEsp32I2sBusN,
    }[config[CONF_BUS]]
    # Some chips are only aliases
    chip = {
        CHIP_WS2811: CHIP_WS2812X,
        CHIP_WS2813: CHIP_WS2812X,
        CHIP_LC8812: CHIP_SK6812,
        CHIP_WS2812: CHIP_800KBPS,
    }.get(chip, chip)

    lookup = {
        CHIP_WS2812X: (neo_ns.NeoEsp32I2sSpeedWs2812x, False),
        CHIP_SK6812: (neo_ns.NeoEsp32I2sSpeedSk6812, False),
        CHIP_TM1814: (neo_ns.NeoEsp32I2sSpeedTm1814, True),
        CHIP_TM1914: (neo_ns.NeoEsp32I2sSpeedTm1914, True),
        CHIP_TM1829: (neo_ns.NeoEsp32I2sSpeedTm1829, True),
        CHIP_800KBPS: (neo_ns.NeoEsp32I2sSpeed800Kbps, False),
        CHIP_400KBPS: (neo_ns.NeoEsp32I2sSpeed400Kbps, False),
        CHIP_APA106: (neo_ns.NeoEsp32I2sSpeedApa106, False),
    }
    speed, inv_inverted = lookup[chip]
    # For tm variants opposite of inverted is needed
    inv = {
        False: neo_ns.NeoEsp32I2sNotInverted,
        True: neo_ns.NeoEsp32I2sInverted,
    }[inverted != inv_inverted]
    return neo_ns.NeoEsp32I2sMethodBase.template(speed, bus, inv)


def _spi_to_code(config, chip: str, inverted: bool):
    # https://github.com/Makuna/NeoPixelBus/blob/master/src/internal/TwoWireSpiImple.h
    spi_imple = {
        None: neo_ns.TwoWireSpiImple,
        SPI_BUS_VSPI: neo_ns.TwoWireSpiImple,
        SPI_BUS_HSPI: neo_ns.TwoWireHspiImple,
    }[config.get(CONF_BUS)]
    spi_speed = {
        40e6: neo_ns.SpiSpeed40Mhz,
        20e6: neo_ns.SpiSpeed20Mhz,
        10e6: neo_ns.SpiSpeed10Mhz,
        5e6: neo_ns.SpiSpeed5Mhz,
        2e6: neo_ns.SpiSpeed2Mhz,
        1e6: neo_ns.SpiSpeed1Mhz,
        500e3: neo_ns.SpiSpeed500Khz,
    }[config[CONF_SPEED]]
    chip_method_base = {
        CHIP_DOTSTAR: neo_ns.DotStarMethodBase,
        CHIP_LPD6803: neo_ns.Lpd6803MethodBase,
        CHIP_LPD8806: neo_ns.Lpd8806MethodBase,
        CHIP_WS2801: neo_ns.Ws2801MethodBase,
        CHIP_P9813: neo_ns.P9813MethodBase,
    }[chip]
    return chip_method_base.template(spi_imple.template(spi_speed))


def _spi_extra_validate(config):
    if CORE.is_esp32:
        return

    if config[CONF_DATA_PIN] != 13 and config[CONF_CLOCK_PIN] != 14:
        raise cv.Invalid(
            "SPI only supports pins GPIO13 for data and GPIO14 for clock on ESP8266"
        )


@dataclass
class MethodDescriptor:
    method_schema: Any
    to_code: Any
    supported_chips: list[str]
    extra_validate: Any = None


METHODS = {
    METHOD_BIT_BANG: MethodDescriptor(
        method_schema={},
        to_code=_bit_bang_to_code,
        extra_validate=_bit_bang_extra_validate,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_ESP8266_UART: MethodDescriptor(
        method_schema=cv.All(
            cv.only_on_esp8266,
            {
                cv.Optional(CONF_ASYNC, default=False): cv.boolean,
                cv.Optional(CONF_BUS, default=1): cv.int_range(min=0, max=1),
            },
        ),
        extra_validate=_esp8266_uart_extra_validate,
        to_code=_esp8266_uart_to_code,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_ESP8266_DMA: MethodDescriptor(
        method_schema=cv.All(cv.only_on_esp8266, {}),
        extra_validate=_esp8266_dma_extra_validate,
        to_code=_esp8266_dma_to_code,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_ESP32_RMT: MethodDescriptor(
        method_schema=cv.All(
            cv.only_on_esp32,
            {
                cv.Optional(
                    CONF_CHANNEL, default=_esp32_rmt_default_channel
                ): _validate_esp32_rmt_channel,
            },
        ),
        to_code=_esp32_rmt_to_code,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_ESP32_I2S: MethodDescriptor(
        method_schema=cv.All(
            cv.only_on_esp32,
            {
                cv.Optional(
                    CONF_BUS, default=_esp32_i2s_default_bus
                ): _validate_esp32_i2s_bus,
            },
        ),
        to_code=_esp32_i2s_to_code,
        supported_chips=ONE_WIRE_CHIPS,
    ),
    METHOD_SPI: MethodDescriptor(
        method_schema={
            cv.Optional(CONF_BUS): cv.All(
                cv.only_on_esp32, cv.one_of(SPI_BUS_VSPI, SPI_BUS_HSPI, lower=True)
            ),
            cv.Optional(CONF_SPEED, default="10MHz"): cv.All(
                cv.frequency, cv.one_of(*SPI_SPEEDS)
            ),
        },
        to_code=_spi_to_code,
        extra_validate=_spi_extra_validate,
        supported_chips=TWO_WIRE_CHIPS,
    ),
}
