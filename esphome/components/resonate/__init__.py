"""Resonate Media Player Setup."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TASK_STACK_IN_PSRAM, PLATFORM_ESP32
from esphome.core import CORE

# json handles server messages, mdns for autodiscovering, media player for stream commands (for now is autoloaded), psram for memory
AUTO_LOAD = ["json", "mdns", "media_player", "psram"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["network"]

CONF_ON_SERVER_SETTINGS = "on_server_settings"
CONF_OPTIMIZE_WIFI = "optimize_wifi"


CONF_KALMAN_PROCESS_ERROR = "kalman_process_error"
CONF_KALMAN_FORGET_FACTOR = "kalman_forget_factor"


CONF_RESONATE_ID = "resonate_id"

resonate_ns = cg.esphome_ns.namespace("resonate")
ResonateHub = resonate_ns.class_(
    "ResonateHub",
    cg.Component,
)


PublishClientSettingsAction = resonate_ns.class_(
    "PublishClientSettingsAction",
    automation.Action,
    cg.Parented.template(ResonateHub),
)

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ResonateHub),
            cv.Optional(CONF_OPTIMIZE_WIFI, default=True): cv.boolean,
            cv.SplitDefault(CONF_TASK_STACK_IN_PSRAM, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
            cv.Optional(CONF_KALMAN_PROCESS_ERROR, default=0.01): cv.float_,
            cv.Optional(CONF_KALMAN_FORGET_FACTOR, default=1.001): cv.float_,
        }
    ),
    cv.only_on([PLATFORM_ESP32]),
    cv.only_with_esp_idf,  # TODO: Is this necessary? I'm adding esp-libopus as an IDF component
)


async def to_code(config):
    cg.add_define("USE_RESONATE", True)  # for MDNS

    # TODO: Opus should be included with the audio component. We should also only define FLAC support if we have components that require the audio stream
    cg.add_define("USE_AUDIO_FLAC_SUPPORT", True)
    if CORE.using_esp_idf:
        esp32.add_idf_component(
            name="esp-libopus",
            repo="https://github.com/kahrendt/esp-libopus",
            ref="main",
        )

    if CORE.using_esp_idf and config[CONF_OPTIMIZE_WIFI]:
        # Based on https://github.com/espressif/esp-idf/blob/release/v5.4/examples/wifi/iperf/sdkconfig.defaults.esp32
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM", 16)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM", 64)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM", 64)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_TX_ENABLED", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_TX_BA_WIN", 32)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_RX_ENABLED", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_RX_BA_WIN", 32)

        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCP_SND_BUF_DEFAULT", 65534)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCP_WND_DEFAULT", 65534)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCP_RECVMBOX_SIZE", 64)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_RECVMBOX_SIZE", 64)

        # Allocate wifi buffers in PSRAM
        esp32.add_idf_sdkconfig_option("CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP", True)

        esp32.add_idf_sdkconfig_option("CONFIG_HTTPD_WS_SUPPORT", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_kalman_process_error(config[CONF_KALMAN_PROCESS_ERROR]))
    cg.add(var.set_kalman_forget_factor(config[CONF_KALMAN_FORGET_FACTOR]))

    if task_stack_in_psram := config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(task_stack_in_psram))
        if task_stack_in_psram:
            esp32.add_idf_sdkconfig_option(
                "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
            )
