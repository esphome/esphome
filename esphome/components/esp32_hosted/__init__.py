import math
import os
from pathlib import Path

from esphome import pins
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_FREQUENCY,
    CONF_RESET_PIN,
    CONF_TYPE,
    CONF_VARIANT,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
)
from esphome.core import CORE

CODEOWNERS = ["@swoboda1337"]

CONF_ACTIVE_HIGH = "active_high"

CONF_SPI_FULL_DUPLEX = "spi-full-duplex"
CONF_SDIO_4BITS = "sdio-4-bits"

BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_VARIANT): cv.one_of(*esp32.VARIANTS, upper=True),
        cv.Required(CONF_ACTIVE_HIGH): cv.boolean,
        cv.Required(CONF_RESET_PIN): pins.internal_gpio_output_pin_number,
    }
)


def _are_closest_possible_floats(a: float, b: float):
    return math.nextafter(a, b) == b


CONF_CMD_PIN = "cmd_pin"
CONF_D0_PIN = "d0_pin"
CONF_D1_PIN = "d1_pin"
CONF_D2_PIN = "d2_pin"
CONF_D3_PIN = "d3_pin"
CONF_SLOT = "slot"

SDIO_4BITS_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D0_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D1_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D2_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D3_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_SLOT, default=1): cv.int_range(min=0, max=1),
    }
)

# available options can be found on the https://github.com/espressif/esp-hosted-mcu/
# repo. Use the following command to extract constants for full duplex SPI:
# grep -oP 'ESP_HOSTED_SPI_(?!HD_)[A-Za-z_]+' Kconfig | sort | uniq

CONF_HANDSHAKE_PIN = "handshake_pin"
CONF_DATA_READY_PIN = "data_ready_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_MISO_PIN = "miso_pin"
CONF_CS_PIN = "cs_pin"
CONF_HANDSHAKE_ACTIVE_HIGH = "handshake_pin_active_high"
CONF_DATA_READY_ACTIVE_HIGH = "data_ready_pin_active_high"
CONF_SPI_MODE = "spi_mode"


def validate_variant_dependent_options(config):
    variant = config[CONF_VARIANT]
    mega = 1e6
    default_spi_mode = 2 if variant == esp32.VARIANT_ESP32 else 3
    max_spi_frequency = 10 * mega if variant == esp32.VARIANT_ESP32 else 40 * mega
    set_frequency = config.get(CONF_FREQUENCY, float(10 * mega))
    if max_spi_frequency < set_frequency:
        raise cv.Invalid(
            f"Max SPI frequency for variant {variant} is {max_spi_frequency / mega} MHz",
            [CONF_FREQUENCY],
        )
    if set_frequency < 1 * mega:
        raise cv.Invalid("Minimum SPI frequency is 1 MHz", [CONF_FREQUENCY])
    if not _are_closest_possible_floats(
        set_frequency / mega, float(round(set_frequency / mega))
    ):
        raise cv.Invalid(
            "Configured frequency must be integer values of MHz", [CONF_FREQUENCY]
        )
    return {CONF_SPI_MODE: default_spi_mode, CONF_FREQUENCY: set_frequency} | config


SPI_FD_SCHEMA = cv.All(
    BASE_SCHEMA.extend(
        {
            cv.Required(CONF_HANDSHAKE_PIN): pins.internal_gpio_input_pin_number,
            cv.Required(CONF_DATA_READY_PIN): pins.internal_gpio_input_pin_number,
            cv.Required(CONF_MOSI_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_MISO_PIN): pins.internal_gpio_input_pin_number,
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_HANDSHAKE_ACTIVE_HIGH, default=True): cv.boolean,
            cv.Optional(CONF_DATA_READY_ACTIVE_HIGH, default=True): cv.boolean,
            # Look these up on https://esphome.io/components/spi/#supported-modes
            # it depends on the esp32 variant used. ESP32 uses mode 2, the rest
            # uses mode 3, however I think these can be configured on the slave
            # device.
            cv.Optional(CONF_SPI_MODE): cv.int_range(min=0, max=3),
            # These will be converted to integer values, so it might
            # be worth it to error out if the user provides something like
            # 9.5 MHz
            cv.Optional(CONF_FREQUENCY): cv.All(
                cv.frequency,
            ),
            # Both TX and RX queue can be configured, same as for SDIO and UART,
            # the only communication platform that doesn't have that is
            # half duplex SPI
            # TODO
            # CONFIG_ESP_HOSTED_SPI_TX_Q_SIZE
            # CONFIG_ESP_HOSTED_SPI_RX_Q_SIZE
        }
    ),
    validate_variant_dependent_options,
)

CONFIG_SCHEMA = cv.typed_schema(
    {CONF_SDIO_4BITS: SDIO_4BITS_SCHEMA, CONF_SPI_FULL_DUPLEX: SPI_FD_SCHEMA},
    default_type=CONF_SDIO_4BITS,
)


def _configure_sdio_4bits(config):
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_SDIO_SLOT_{config[CONF_SLOT]}",
        True,
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CLK_SLOT_{config[CONF_SLOT]}",
        config[CONF_CLK_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CMD_SLOT_{config[CONF_SLOT]}",
        config[CONF_CMD_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D0_SLOT_{config[CONF_SLOT]}",
        config[CONF_D0_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D1_4BIT_BUS_SLOT_{config[CONF_SLOT]}",
        config[CONF_D1_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D2_4BIT_BUS_SLOT_{config[CONF_SLOT]}",
        config[CONF_D2_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D3_4BIT_BUS_SLOT_{config[CONF_SLOT]}",
        config[CONF_D3_PIN],
    )
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_CUSTOM_SDIO_PINS", True)


def _configure_spi_fd(config):
    options = {
        "CONFIG_ESP_HOSTED_SPI_HOST_INTERFACE": True,
        "CONFIG_ESP_HOSTED_SPI_MODE": config[CONF_SPI_MODE],
        "CONFIG_ESP_HOSTED_SPI_CLK_FREQ": str(config[CONF_FREQUENCY] // 1e6),
        "CONFIG_ESP_HOSTED_SPI_GPIO_MOSI": config[CONF_MOSI_PIN],
        "CONFIG_ESP_HOSTED_SPI_GPIO_MISO": config[CONF_MISO_PIN],
        "CONFIG_ESP_HOSTED_SPI_GPIO_CLK": config[CONF_CLK_PIN],
        "CONFIG_ESP_HOSTED_SPI_GPIO_CS": config[CONF_CS_PIN],
        "CONFIG_ESP_HOSTED_SPI_GPIO_HANDSHAKE": config[CONF_HANDSHAKE_PIN],
        f"CONFIG_ESP_HOSTED_HS_ACTIVE_{'HIGH' if config[CONF_HANDSHAKE_ACTIVE_HIGH] else 'LOW'}": True,
        "CONFIG_ESP_HOSTED_SPI_GPIO_DATA_READY": config[CONF_DATA_READY_PIN],
        f"CONFIG_ESP_HOSTED_DR_ACTIVE_{'HIGH' if config[CONF_DATA_READY_ACTIVE_HIGH] else 'LOW'}": True,
        # TODO
        # CONFIG_ESP_HOSTED_SPI_TX_Q_SIZE
        # CONFIG_ESP_HOSTED_SPI_RX_Q_SIZE
    }
    for k, v in options.items():
        esp32.add_idf_sdkconfig_option(k, v)


async def to_code(config):
    variant = config[CONF_VARIANT]
    type_prefix = {CONF_SDIO_4BITS: "SDIO", CONF_SPI_FULL_DUPLEX: "SPI"}[
        config[CONF_TYPE]
    ]
    if config[CONF_ACTIVE_HIGH]:
        esp32.add_idf_sdkconfig_option(
            f"CONFIG_ESP_HOSTED_{type_prefix}_RESET_ACTIVE_HIGH",
            True,
        )
    else:
        esp32.add_idf_sdkconfig_option(
            f"CONFIG_ESP_HOSTED_{type_prefix}_RESET_ACTIVE_LOW",
            True,
        )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_{type_prefix}_GPIO_RESET_SLAVE",  # NOLINT
        config[CONF_RESET_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_SLAVE_IDF_TARGET_{variant}",  # NOLINT
        True,
    )
    if config[CONF_TYPE] == CONF_SDIO_4BITS:
        _configure_sdio_4bits(config)
    elif config[CONF_TYPE] == CONF_SPI_FULL_DUPLEX:
        _configure_spi_fd(config)
    else:
        raise NotImplementedError
    # Can we get this list from somewhere or do we need to hardcode it?
    variants_with_he_support = (
        esp32.VARIANT_ESP32C6,
        esp32.VARIANT_ESP32C5,
        esp32.VARIANT_ESP32C61,
        esp32.VARIANT_ESP32P4,
    )
    if esp32.get_esp32_variant() not in variants_with_he_support:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_ENABLE_ITWT", False)

    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    os.environ["ESP_IDF_VERSION"] = f"{framework_ver.major}.{framework_ver.minor}"
    if framework_ver >= cv.Version(5, 5, 0):
        esp32.add_idf_component(name="espressif/esp_wifi_remote", ref="1.2.2")
        esp32.add_idf_component(name="espressif/eppp_link", ref="1.1.3")
        esp32.add_idf_component(name="espressif/esp_hosted", ref="2.7.0")
    else:
        esp32.add_idf_component(name="espressif/esp_wifi_remote", ref="0.13.0")
        esp32.add_idf_component(name="espressif/eppp_link", ref="0.2.0")
        esp32.add_idf_component(name="espressif/esp_hosted", ref="2.0.11")
    esp32.add_extra_script(
        "post",
        "esp32_hosted.py",
        Path(__file__).parent / "esp32_hosted.py.script",
    )
