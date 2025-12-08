import os
from pathlib import Path

from esphome import pins
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
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


SPI_FD_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.Required(CONF_HANDSHAKE_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_DATA_READY_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_MOSI_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_MISO_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_number,
        # TODO fill this out
    }
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
    raise NotImplementedError


async def to_code(config):
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
        f"CONFIG_SLAVE_IDF_TARGET_{config[CONF_VARIANT]}",  # NOLINT
        True,
    )
    if config[CONF_TYPE] == CONF_SDIO_4BITS:
        _configure_sdio_4bits(config)
    elif config[CONF_TYPE] == CONF_SPI_FULL_DUPLEX:
        _configure_spi_fd(config)
    else:
        raise NotImplementedError

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
