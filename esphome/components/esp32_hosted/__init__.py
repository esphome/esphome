import os
from pathlib import Path

from esphome import pins
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_RESET_PIN,
    CONF_VARIANT,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
)
from esphome.core import CORE

CODEOWNERS = ["@swoboda1337"]

CONF_ACTIVE_HIGH = "active_high"
CONF_CMD_PIN = "cmd_pin"
CONF_D0_PIN = "d0_pin"
CONF_D1_PIN = "d1_pin"
CONF_D2_PIN = "d2_pin"
CONF_D3_PIN = "d3_pin"
CONF_SLOT = "slot"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_VARIANT): cv.one_of(*esp32.VARIANTS, upper=True),
            cv.Required(CONF_ACTIVE_HIGH): cv.boolean,
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CMD_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_D0_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_D1_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_D2_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_D3_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_RESET_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_SLOT, default=1): cv.int_range(min=0, max=1),
        }
    ),
)


async def to_code(config):
    if config[CONF_ACTIVE_HIGH]:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_HIGH",
            True,
        )
    else:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW",
            True,
        )
    esp32.add_idf_sdkconfig_option(
        "CONFIG_ESP_HOSTED_SDIO_GPIO_RESET_SLAVE",  # NOLINT
        config[CONF_RESET_PIN],
    )
    esp32.add_idf_sdkconfig_option(
        f"CONFIG_SLAVE_IDF_TARGET_{config[CONF_VARIANT]}",  # NOLINT
        True,
    )
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
