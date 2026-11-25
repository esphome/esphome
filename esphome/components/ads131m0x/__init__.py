from esphome import pins
import esphome.codegen as cg
from esphome.components import spi
from esphome.components.const import CONF_DRDY_PIN, CONF_WINDOW
from esphome.components.esp32 import VARIANT_ESP32P4, get_esp32_variant
import esphome.config_validation as cv
from esphome.const import (
    CONF_CS_PIN,
    CONF_DATA_RATE,
    CONF_GAIN,
    CONF_ID,
    CONF_MODEL,
    CONF_POWER_MODE,
    CONF_RESET_PIN,
    CONF_SAMPLE_RATE,
    CONF_SPI_ID,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@Nawo"]
DEPENDENCIES = ["spi"]
DOMAIN = "ads131m0x"
MULTI_CONF = True

CONF_ADS131M0X_ID = "ads131m0x_id"
CONF_CLOCK_FREQUENCY = "clock_frequency"

ads131m0x_ns = cg.esphome_ns.namespace("ads131m0x")
ADS131M0x = ads131m0x_ns.class_("ADS131M0x", cg.Component, spi.SPIDevice)

# Number of input channels per model. ADS131M03/M06 are left out until their CLOCK register
# layout (reset values 080Eh / 7F0Eh) is verified on hardware.
MODELS = {
    "ADS131M02": 2,
    "ADS131M04": 4,
    "ADS131M08": 8,
}

# Datasheet SBAS950B, Recommended Operating Conditions: f_CLKIN 0.3 MHz up to these limits
MIN_CLOCK_HZ = 0.3e6
MAX_CLOCK_HZ = {
    "VERY_LOW_POWER": 2.08e6,
    "LOW_POWER": 4.15e6,
    "HIGH_RESOLUTION": 8.4e6,
}
# In high-resolution mode f_CLKIN is limited further when any channel uses a gain above 2
MAX_CLOCK_HZ_HIGH_GAIN = 8.2e6

ADS131M0xPowerMode = ads131m0x_ns.enum("ADS131M0xPowerMode")
POWER_MODES = {
    "VERY_LOW_POWER": ADS131M0xPowerMode.ADS131M0X_POWER_MODE_VERY_LOW_POWER,
    "LOW_POWER": ADS131M0xPowerMode.ADS131M0X_POWER_MODE_LOW_POWER,
    "HIGH_RESOLUTION": ADS131M0xPowerMode.ADS131M0X_POWER_MODE_HIGH_RESOLUTION,
}

# PGA gain -> GAINn register code
GAINS = {1: 0, 2: 1, 4: 2, 8: 3, 16: 4, 32: 5, 64: 6, 128: 7}
# CLOCK.OSR register code -> oversampling ratio; data rate = f_CLKIN / 2 / OSR.
# The register table lists 16256 for code 7 while the data rate table uses 16384 (250 SPS at
# 8.192 MHz); both are within SAMPLE_RATE_TOLERANCE of each other.
OSR_CODES = {0: 128, 1: 256, 2: 512, 3: 1024, 4: 2048, 5: 4096, 6: 8192, 7: 16256}
# Relative tolerance when matching sample_rate to an achievable data rate
SAMPLE_RATE_TOLERANCE = 0.01
# Datasheet SBAS950B, Timing Requirements: t_c(SC) of at least 40 ns with DVDD 2.7 V to 3.6 V (3.3 V logic)
MAX_SCLK_HZ = 25e6
# The transfer length is padded like DMA_ALIGNMENT in ads131m0x.cpp: to the 64-byte L1 cache line where internal
# RAM is cached (ESP32-P4), elsewhere to whole 32-bit words.
DMA_ALIGNMENT_BYTES = {VARIANT_ESP32P4: 64}
DEFAULT_DMA_ALIGNMENT_BYTES = 4
# Only mode 1 (CPOL = 0, CPHA = 1): the chip latches DIN on the falling edge of SCLK
SPI_MODES = {
    key: value for key, value in spi.SPI_MODE_OPTIONS.items() if key in ("MODE1", "1")
}
# A frame must fit in this fraction of a sample period, leaving time for the task wake-up and other bus users
MAX_FRAME_TIME_FRACTION = 0.5
# The task accumulates squared 24-bit codes in uint64: at most 2^46 * 2^17 = 2^63.
MAX_WINDOW_SAMPLES = 1 << 17


def _osr_code(clock_hz: float, sample_rate: float) -> int:
    for code, osr in OSR_CODES.items():
        rate = clock_hz / 2 / osr
        if abs(rate - sample_rate) <= rate * SAMPLE_RATE_TOLERANCE:
            return code
    achievable = ", ".join(f"{clock_hz / 2 / osr:g}" for osr in OSR_CODES.values())
    raise cv.Invalid(
        f"sample_rate {sample_rate:g} Hz cannot be reached with a {clock_hz / 1e6:g} MHz clock; "
        f"achievable rates: {achievable}",
        [CONF_SAMPLE_RATE],
    )


def _validate(config: ConfigType) -> ConfigType:
    channels = MODELS[config[CONF_MODEL]]
    gain = config[CONF_GAIN]
    if not isinstance(gain, list):
        config[CONF_GAIN] = gain = [gain] * channels
    elif len(gain) != channels:
        raise cv.Invalid(
            f"{config[CONF_MODEL]} has {channels} channels, so gain needs {channels} values",
            [CONF_GAIN],
        )
    clock_hz = config[CONF_CLOCK_FREQUENCY]
    power_mode = config[CONF_POWER_MODE]
    max_clock_hz = MAX_CLOCK_HZ[power_mode]
    if power_mode == "HIGH_RESOLUTION" and max(gain) > 2:
        max_clock_hz = MAX_CLOCK_HZ_HIGH_GAIN
    if not MIN_CLOCK_HZ <= clock_hz <= max_clock_hz:
        raise cv.Invalid(
            f"clock_frequency must be between {MIN_CLOCK_HZ / 1e6:g} and {max_clock_hz / 1e6:g} MHz "
            f"in {power_mode.lower()} mode with this gain",
            [CONF_CLOCK_FREQUENCY],
        )
    osr = OSR_CODES[_osr_code(clock_hz, config[CONF_SAMPLE_RATE])]
    window_samples = clock_hz / 2 / osr * config[CONF_WINDOW].total_milliseconds / 1000
    data_rate = config[CONF_DATA_RATE]
    if data_rate > MAX_SCLK_HZ:
        raise cv.Invalid(
            f"data_rate must be at most {MAX_SCLK_HZ / 1e6:g} MHz (minimum SCLK period of the device)",
            [CONF_DATA_RATE],
        )
    # Longest frame: response + channels + CRC, padded for the DMA as in setup()
    alignment = DMA_ALIGNMENT_BYTES.get(
        get_esp32_variant(), DEFAULT_DMA_ALIGNMENT_BYTES
    )
    frame_bits = -(-(2 + channels) * 3 // alignment) * alignment * 8
    frame_s = frame_bits / data_rate
    sample_period_s = 2 * osr / clock_hz
    if frame_s > sample_period_s * MAX_FRAME_TIME_FRACTION:
        raise cv.Invalid(
            f"data_rate is too slow for this sample_rate: a frame takes {frame_s * 1e6:.0f} us, "
            f"the sample period is {sample_period_s * 1e6:.0f} us; raise data_rate or lower sample_rate",
            [CONF_DATA_RATE],
        )
    if window_samples > MAX_WINDOW_SAMPLES:
        raise cv.Invalid(
            f"window holds {window_samples:.0f} samples, the maximum is {MAX_WINDOW_SAMPLES}",
            [CONF_WINDOW],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS131M0x),
            cv.Required(CONF_MODEL): cv.enum(MODELS, upper=True),
            cv.Required(CONF_DRDY_PIN): pins.internal_gpio_input_pin_schema,
            # Driven from the sampling task: an I/O expander pin would be written concurrently with the main loop
            cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_CLOCK_FREQUENCY, default="8.192MHz"): cv.frequency,
            cv.Optional(CONF_POWER_MODE, default="HIGH_RESOLUTION"): cv.enum(
                POWER_MODES, upper=True, space="_"
            ),
            cv.Optional(CONF_SAMPLE_RATE, default="4000Hz"): cv.frequency,
            cv.Optional(
                CONF_WINDOW, default="100ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_GAIN, default=1): cv.Any(
                cv.one_of(*GAINS, int=True), cv.ensure_list(cv.one_of(*GAINS, int=True))
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        spi.spi_device_schema(
            cs_pin_required=True, default_data_rate=8e6, default_mode="MODE1"
        )
    )
    .extend(
        {
            # Toggled from the sampling task for every frame, like the reset pin
            cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(spi.CONF_SPI_MODE, default="MODE1"): cv.enum(
                SPI_MODES, upper=True
            ),
            # Would add and remove the SPI device for every conversion
            cv.Optional(spi.CONF_RELEASE_DEVICE): cv.invalid(
                "release_device is not supported: the device is read continuously"
            ),
        }
    ),
    cv.only_on_esp32,
    _validate,
)


_FINAL_VALIDATE_SPI_PINS = spi.final_validate_device_schema(
    DOMAIN, require_mosi=True, require_miso=True
)


def _final_validate(config: ConfigType) -> ConfigType:
    _FINAL_VALIDATE_SPI_PINS(config)
    # The sampling task shares the bus with the main loop. Only the hardware SPI driver locks the bus for each
    # transaction; a software bus could interleave this chip's frames with another device's transfer.
    bus = fv.get_declaring_config(config[CONF_SPI_ID])
    if bus.get(spi.CONF_INTERFACE_INDEX) is None:
        raise cv.Invalid(
            "ads131m0x needs a hardware SPI bus; set 'interface: hardware' on the spi bus",
            [CONF_SPI_ID],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_num_channels(MODELS[config[CONF_MODEL]]))

    drdy = await cg.gpio_pin_expression(config[CONF_DRDY_PIN])
    cg.add(var.set_drdy_pin(drdy))
    if (reset_pin := config.get(CONF_RESET_PIN)) is not None:
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))

    clock_hz = config[CONF_CLOCK_FREQUENCY]
    osr_code = _osr_code(clock_hz, config[CONF_SAMPLE_RATE])
    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))
    cg.add(var.set_osr_code(osr_code))
    cg.add(var.set_sample_rate(clock_hz / 2 / OSR_CODES[osr_code]))
    cg.add(var.set_window_ms(config[CONF_WINDOW].total_milliseconds))

    for channel, gain in enumerate(config[CONF_GAIN]):
        cg.add(var.set_gain(channel, GAINS[gain]))
