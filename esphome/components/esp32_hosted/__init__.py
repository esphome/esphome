import logging
import os
from pathlib import Path

from esphome import pins
from esphome.components import esp32
from esphome.components.const import CONF_SLOT, CONF_USE_PSRAM
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
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@swoboda1337"]
DEPENDENCIES = ["esp32"]
DOMAIN = "esp32_hosted"
# esp32_ble raises the task watchdog around the remote BT controller bring-up
AUTO_LOAD = ["watchdog"]

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
CONF_SPI_MODE = "spi_mode"

# ESP-NOW-over-hosted shim (esp_now_hosted.cpp). esp-hosted proxies esp_wifi.h
# but not esp_now.h (espressif/esp-hosted-mcu#19), and esp_wifi_remote injects
# the esp_now.h header on the ESP32-P4 host with no implementation, leaving the
# esp_now_* symbols undefined at link. On a P4 host, esp_now_hosted.cpp DEFINES
# those symbols and forwards each call to the co-processor over esp-hosted's
# CustomRpc "peer data transfer" channel, so ESPHome's `espnow` component links
# and runs unchanged (proven on a Tab5, 2026-07-20). The .cpp is guarded to
# CONFIG_IDF_TARGET_ESP32P4 so it compiles to nothing on hosts with a native
# ESP-NOW stack. CustomRpc needs these two host-side Kconfig options. Host
# registers 3 handlers (RESP, RECV, SEND); the coprocessor registers 1 (REQ);
# we ask for 8 to leave room for other CustomRpc extensions alongside.
#
# The coprocessor must run the matching custom firmware (a parallel effort in
# esphome/esp-hosted-firmware). esp_now_hosted_rpc.h here is the canonical copy
# of the wire contract and MUST stay byte-identical to the copy that coprocessor
# firmware uses — the packed structs are the on-wire layout, so any divergence
# silently corrupts every ESP-NOW frame.
_MAX_CUSTOM_MSG_HANDLERS = 8

# Shared fields for both transport modes
BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_VARIANT): cv.one_of(*esp32.VARIANTS, upper=True),
        cv.Optional(CONF_ACTIVE_HIGH): cv.boolean,
        cv.Required(CONF_RESET_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_USE_PSRAM, default=False): cv.boolean,
    }
)

SDIO_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D0_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D1_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_D2_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_D3_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_BUS_WIDTH, default=4): cv.one_of(1, 4, int=True),
        cv.Optional(CONF_SLOT, default=1): cv.int_range(min=0, max=1),
        cv.Optional(CONF_SDIO_FREQUENCY, default="40MHz"): cv.All(
            cv.frequency, cv.Range(min=400e3, max=50e6)
        ),
    }
)


def _validate_active_high(config: ConfigType) -> ConfigType:
    # Remove before 2027.5.0
    if (active_high := config.get(CONF_ACTIVE_HIGH)) is None:
        return config
    if not active_high:
        raise cv.Invalid(
            "esp_hosted always parks the reset line high with a low pulse, so "
            "'active_high: false' is not supported.",
            path=[CONF_ACTIVE_HIGH],
        )
    _LOGGER.warning(
        "'active_high' is deprecated and ignored, remove it from esp32_hosted. "
        "Removed in 2027.5.0"
    )
    return config


def _validate_sdio(config: ConfigType) -> ConfigType:
    if config[CONF_BUS_WIDTH] == 4:
        for pin in (CONF_D2_PIN, CONF_D3_PIN):
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


def _validate_spi(config: ConfigType) -> ConfigType:
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
        "sdio": cv.All(SDIO_SCHEMA, _validate_sdio, _validate_active_high),
        "spi": cv.All(SPI_SCHEMA, _validate_spi, _validate_active_high),
    },
    default_type="sdio",
)


def _final_validate(config: ConfigType) -> None:
    if (idf_ver := esp32.idf_version()) < cv.Version(5, 5, 0):
        raise cv.Invalid(
            f"esp32_hosted requires ESP-IDF 5.5 or newer, got {idf_ver}. "
            "Remove the framework version from your configuration to use the "
            "recommended version, or pin a version at or above 5.5."
        )


FINAL_VALIDATE_SCHEMA = _final_validate


def _configure_sdio(config: ConfigType) -> None:
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_HOST_TRANSPORT_BUS_SDIO", True)
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_SDIO_SLOT_{config[CONF_SLOT]}",
        True,
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_HOST_SDIO_BUS_WIDTH_{config[CONF_BUS_WIDTH]}",
        True,
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_CLK", config[CONF_CLK_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_CMD", config[CONF_CMD_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D0", config[CONF_D0_PIN]
    )
    # D1 carries the card interrupt on a 1-bit bus too
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D1", config[CONF_D1_PIN]
    )
    if config[CONF_BUS_WIDTH] == 4:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D2", config[CONF_D2_PIN]
        )
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_HOST_SDIO_PIN_D3", config[CONF_D3_PIN]
        )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SDIO_CLK_KHZ",
        int(config[CONF_SDIO_FREQUENCY] // 1000),
    )


def _configure_spi(config: ConfigType) -> None:
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_HOST_TRANSPORT_BUS_SPI", True)
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SPI_MODE", config[CONF_SPI_MODE]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SPI_CLK_MHZ", int(config[CONF_FREQUENCY] // 1e6)
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SPI_MOSI_GPIO", config[CONF_MOSI_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SPI_MISO_GPIO", config[CONF_MISO_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SPI_CLK_GPIO", config[CONF_CLK_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_SPI_CS_GPIO", config[CONF_CS_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_HANDSHAKE_GPIO", config[CONF_HANDSHAKE_PIN]
    )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_DATA_READY_GPIO", config[CONF_DATA_READY_PIN]
    )
    # Handshake and data_ready polarity
    if config[CONF_HANDSHAKE_ACTIVE_HIGH]:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_SPI_HANDSHAKE_ACTIVE_HIGH", True
        )
    else:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_SPI_HANDSHAKE_ACTIVE_LOW", True
        )
    if config[CONF_DATA_READY_ACTIVE_HIGH]:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_SPI_DATAREADY_ACTIVE_HIGH", True
        )
    else:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_SPI_DATAREADY_ACTIVE_LOW", True
        )


async def to_code(config: ConfigType) -> None:
    add_define("USE_ESP32_HOSTED")
    # The board choice defaults to an Espressif dev board, which locks every
    # pin's Kconfig range to that board and silently drops other values
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_P4_DEV_BOARD_NONE", True)
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_HOST_RESET_GPIO", config[CONF_RESET_PIN]
    )
    # Co-processor variant
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_ESP_HOSTED_CP_TARGET_{config[CONF_VARIANT]}",
        True,
    )
    if config[CONF_TYPE] == "sdio":
        _configure_sdio(config)
    else:
        _configure_spi(config)
    if esp32.get_esp32_variant() == esp32.VARIANT_ESP32P4:
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_HOST_FEAT_PEER_DATA", True)
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_HOST_FEAT_PEER_DATA_MAX_CUSTOM_MSG_HANDLERS",
            _MAX_CUSTOM_MSG_HANDLERS,
        )
    if config[CONF_USE_PSRAM]:
        esp32.add_idf_sdkconfig_option("CONFIG_EH_HOST_PORT_DMA_PREFER_SPIRAM", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_HOSTED_DFLT_TASK_FROM_SPIRAM", True)

    # ESP-NOW-over-hosted shim: only the radio-less ESP32-P4 host needs it (see
    # the note by _MAX_CUSTOM_MSG_HANDLERS). Enabled for every P4 host, not
    # gated on the `espnow` component being present: the shim is tiny and the
    # esp_now_* symbols/CustomRpc calls it defines require the peer-data Kconfig
    # options (set above) to link whenever esp_now_hosted.cpp compiles (which is
    # on any P4 host), so coupling the two keeps the build consistent. When
    # `espnow` is absent the symbols are simply unused and never register a
    # callback at runtime.
    if esp32.get_esp32_variant() == esp32.VARIANT_ESP32P4:
        add_define("USE_ESP_NOW_HOSTED")
        # esp_now_hosted.cpp includes esp_now.h, which esp_wifi provides
        esp32.include_builtin_idf_component("esp_wifi")

    # Library versions; this component set requires ESP-IDF 5.5 or newer,
    # which is enforced at validation time.
    idf_ver = esp32.idf_version()
    os.environ["ESP_IDF_VERSION"] = f"{idf_ver.major}.{idf_ver.minor}"
    esp32.add_idf_component(name="espressif/esp_wifi_remote", ref="1.6.5")
    esp32.add_idf_component(name="espressif/wifi_remote_over_eppp", ref="0.3.3")
    esp32.add_idf_component(name="espressif/eppp_link", ref="1.1.5")
    esp32.add_idf_component(name="espressif/esp_hosted", ref="3.0.10")
    esp32.add_extra_script(
        "post",
        "esp32_hosted.py",
        Path(__file__).parent / "esp32_hosted.py.script",
    )
