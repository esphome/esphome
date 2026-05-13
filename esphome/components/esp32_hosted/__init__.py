import os
from pathlib import Path

from esphome import pins
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_CS_PIN,
    CONF_FREQUENCY,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_RESET_PIN,
    CONF_TYPE,
    CONF_VARIANT,
)
from esphome.cpp_generator import add_define

CODEOWNERS = ["@swoboda1337"]

CONF_ACTIVE_HIGH = "active_high"
CONF_BUS_WIDTH = "bus_width"
CONF_CMD_PIN = "cmd_pin"
CONF_D0_PIN = "d0_pin"
CONF_D1_PIN = "d1_pin"
CONF_D2_PIN = "d2_pin"
CONF_D3_PIN = "d3_pin"
CONF_DATA_READY_ACTIVE_HIGH = "data_ready_active_high"
CONF_DATA_READY_PIN = "data_ready_pin"
CONF_HANDSHAKE_ACTIVE_HIGH = "handshake_active_high"
CONF_HANDSHAKE_PIN = "handshake_pin"
CONF_SDIO_FREQUENCY = "sdio_frequency"
CONF_SLOT = "slot"
CONF_SPI_MODE = "spi_mode"

# Shared fields for both transport modes
BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_VARIANT): cv.one_of(*esp32.VARIANTS, upper=True),
        cv.Required(CONF_ACTIVE_HIGH): cv.boolean,
        cv.Required(CONF_RESET_PIN): pins.internal_gpio_output_pin_number,
    }
)

SDIO_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D0_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_D1_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_D2_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_D3_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_BUS_WIDTH, default=4): cv.one_of(1, 4, int=True),
        cv.Optional(CONF_SLOT, default=1): cv.int_range(min=0, max=1),
        cv.Optional(CONF_SDIO_FREQUENCY, default="40MHz"): cv.All(
            cv.frequency, cv.Range(min=400e3, max=50e6)
        ),
    }
)


def _validate_sdio(config):
    if config[CONF_BUS_WIDTH] == 4:
        for pin in (CONF_D1_PIN, CONF_D2_PIN, CONF_D3_PIN):
            if pin not in config:
                raise cv.Invalid(
                    f"{pin} is required when bus_width is 4",
                    path=[pin],
                )
    return config


# SPI variant-dependent defaults and limits
_SPI_VARIANT_DEFAULTS = {
    "ESP32": {"spi_mode": 2, "frequency": 10, "max_frequency": 10},
    "ESP32C6": {"spi_mode": 3, "frequency": 26, "max_frequency": 40},
}
_SPI_DEFAULT = {"spi_mode": 3, "frequency": 40, "max_frequency": 40}

SPI_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_MOSI_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_MISO_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_HANDSHAKE_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_DATA_READY_PIN): pins.internal_gpio_input_pin_number,
        cv.Optional(CONF_SPI_MODE): cv.int_range(min=0, max=3),
        cv.Optional(CONF_FREQUENCY): cv.All(cv.frequency, cv.Range(min=1e6, max=40e6)),
        cv.Optional(CONF_HANDSHAKE_ACTIVE_HIGH, default=True): cv.boolean,
        cv.Optional(CONF_DATA_READY_ACTIVE_HIGH, default=True): cv.boolean,
    }
)


def _validate_spi(config):
    variant = config[CONF_VARIANT]
    defaults = _SPI_VARIANT_DEFAULTS.get(variant, _SPI_DEFAULT)

    if CONF_SPI_MODE not in config:
        config[CONF_SPI_MODE] = defaults["spi_mode"]

    if CONF_FREQUENCY not in config:
        config[CONF_FREQUENCY] = float(defaults["frequency"] * 1e6)

    freq_mhz = int(config[CONF_FREQUENCY] // 1e6)
    if freq_mhz > defaults["max_frequency"]:
        raise cv.Invalid(
            f"SPI frequency {freq_mhz}MHz exceeds maximum {defaults['max_frequency']}MHz for {variant}",
            path=[CONF_FREQUENCY],
        )
    return config


CONFIG_SCHEMA = cv.typed_schema(
    {
        "sdio": cv.All(SDIO_SCHEMA, _validate_sdio),
        "spi": cv.All(SPI_SCHEMA, _validate_spi),
    },
    default_type="sdio",
)


def _configure_sdio(config):
    slot = config[CONF_SLOT]
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_SDIO_SLOT_{slot}",
        True,
    )
    if config[CONF_BUS_WIDTH] == 1:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_SDIO_1_BIT_BUS", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS", True)
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CLK_SLOT_{slot}",
        config[CONF_CLK_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_CMD_SLOT_{slot}",
        config[CONF_CMD_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D0_SLOT_{slot}",
        config[CONF_D0_PIN],
    )
    if config[CONF_BUS_WIDTH] == 4:
        esp32.add_idf_sdkconfig_option(
            f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D1_4BIT_BUS_SLOT_{slot}",
            config[CONF_D1_PIN],
        )
        esp32.add_idf_sdkconfig_option(
            f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D2_4BIT_BUS_SLOT_{slot}",
            config[CONF_D2_PIN],
        )
        esp32.add_idf_sdkconfig_option(
            f"CONFIG_ESP_HOSTED_PRIV_SDIO_PIN_D3_4BIT_BUS_SLOT_{slot}",
            config[CONF_D3_PIN],
        )
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_CUSTOM_SDIO_PINS", True)
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SDIO_CLOCK_FREQ_KHZ",
        int(config[CONF_SDIO_FREQUENCY] // 1000),
    )


def _configure_spi(config):
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_SPI_HOST_INTERFACE", True)
    # SPI mode is set via per-variant choice options
    variant = config[CONF_VARIANT]
    mode = config[CONF_SPI_MODE]
    suffix = "ESP32" if variant == "ESP32" else "ESP32XX"
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_SPI_PRIV_MODE_{mode}_{suffix}",
        True,
    )
    # Frequency is set via per-variant options
    freq_mhz = int(config[CONF_FREQUENCY] // 1e6)
    if variant == "ESP32":
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_SPI_FREQ_ESP32", freq_mhz)
    elif variant == "ESP32C6":
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_SPI_FREQ_ESP32C6", freq_mhz)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_SPI_FREQ_ESP32XX", freq_mhz)
    # Pin configuration (use HSPI variant as P4/H2 hosts don't have VSPI)
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SPI_HSPI_GPIO_MOSI", config[CONF_MOSI_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SPI_HSPI_GPIO_MISO", config[CONF_MISO_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SPI_HSPI_GPIO_CLK", config[CONF_CLK_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SPI_HSPI_GPIO_CS", config[CONF_CS_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SPI_GPIO_HANDSHAKE", config[CONF_HANDSHAKE_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SPI_GPIO_DATA_READY", config[CONF_DATA_READY_PIN]
    )
    # Handshake and data_ready polarity
    if config[CONF_HANDSHAKE_ACTIVE_HIGH]:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_HS_ACTIVE_HIGH", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_HS_ACTIVE_LOW", True)
    if config[CONF_DATA_READY_ACTIVE_HIGH]:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_DR_ACTIVE_HIGH", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_DR_ACTIVE_LOW", True)


async def to_code(config):
    add_define("USE_ESP32_HOSTED")
    transport = config[CONF_TYPE]
    transport_prefix = "SDIO" if transport == "sdio" else "SPI"

    # Reset polarity
    if config[CONF_ACTIVE_HIGH]:
        esp32.add_idf_sdkconfig_option(
            f"CONFIG_ESP_HOSTED_{transport_prefix}_RESET_ACTIVE_HIGH", True
        )
    else:
        esp32.add_idf_sdkconfig_option(
            f"CONFIG_ESP_HOSTED_{transport_prefix}_RESET_ACTIVE_LOW", True
        )
    # Reset GPIO
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_{transport_prefix}_GPIO_RESET_SLAVE",  # NOLINT
        config[CONF_RESET_PIN],
    )
    # Slave variant  # NOLINT
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_SLAVE_IDF_TARGET_{config[CONF_VARIANT]}",  # NOLINT
        True,
    )

    # Transport-specific configuration
    if transport == "sdio":
        _configure_sdio(config)
    else:
        _configure_spi(config)

    # Library versions
    idf_ver = esp32.idf_version()
    os.environ["ESP_IDF_VERSION"] = f"{idf_ver.major}.{idf_ver.minor}"
    if idf_ver >= cv.Version(5, 5, 0):
        esp32.add_idf_component(name="espressif/esp_wifi_remote", ref="1.5.1")
        esp32.add_idf_component(name="espressif/wifi_remote_over_eppp", ref="0.3.2")
        esp32.add_idf_component(name="espressif/eppp_link", ref="1.1.5")
        esp32.add_idf_component(name="espressif/esp_hosted", ref="2.12.6")
    else:
        esp32.add_idf_component(name="espressif/esp_wifi_remote", ref="0.13.0")
        esp32.add_idf_component(name="espressif/eppp_link", ref="0.2.0")
        esp32.add_idf_component(name="espressif/esp_hosted", ref="2.0.11")
    esp32.add_extra_script(
        "post",
        "esp32_hosted.py",
        Path(__file__).parent / "esp32_hosted.py.script",
    )
