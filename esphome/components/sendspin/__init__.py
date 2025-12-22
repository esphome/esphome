"""Sendspin Media Player Setup."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32, network, socket, wifi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_TASK_STACK_IN_PSRAM,
    PLATFORM_ESP32,
)
from esphome.core import CORE

# json handles server messages, mdns for autodiscovering, media player for stream commands (for now is autoloaded), psram for memory
AUTO_LOAD = ["json", "mdns", "media_player", "psram"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["network"]

CONF_ON_SERVER_SETTINGS = "on_server_settings"


CONF_KALMAN_PROCESS_ERROR = "kalman_process_error"
CONF_KALMAN_FORGET_FACTOR = "kalman_forget_factor"


CONF_SENDSPIN_ID = "sendspin_id"

sendspin_ns = cg.esphome_ns.namespace("sendspin")
SendspinHub = sendspin_ns.class_(
    "SendspinHub",
    cg.Component,
)


PublishClientSettingsAction = sendspin_ns.class_(
    "PublishClientSettingsAction",
    automation.Action,
    cg.Parented.template(SendspinHub),
)


def _request_high_performance_networking(config):
    """Request high performance networking for Sendspin streaming.

    Also enables wake_loop_threadsafe support for fast defer() callbacks
    from background threads (WebSocket handler, image decoder).
    """
    network.require_high_performance_networking()
    socket.require_wake_loop_threadsafe()
    socket.consume_sockets(1, "sendspin_websocket_server")(
        config
    )  # Currently only supports one connection
    wifi.enable_runtime_power_save_control()  # TODO: Is this safe if wifi isn't configured?
    return config


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SendspinHub),
            cv.SplitDefault(CONF_TASK_STACK_IN_PSRAM, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
            cv.Optional(CONF_KALMAN_PROCESS_ERROR, default=0.01): cv.float_,
            cv.Optional(CONF_KALMAN_FORGET_FACTOR, default=1.001): cv.float_,
            cv.Optional(CONF_BUFFER_SIZE): cv.invalid(
                f"The {CONF_BUFFER_SIZE} option is now set as an option in the Sendspin media_source configuration"
            ),
        }
    ),
    cv.only_on([PLATFORM_ESP32]),
    cv.only_with_esp_idf,  # TODO: Is this still necessary in recent versions of ESPHome?
    _request_high_performance_networking,
)


async def to_code(config):
    cg.add_define("USE_SENDSPIN", True)  # for MDNS

    # TODO: Opus should be included with the audio component. We should also only define FLAC support if we have components that require the audio stream
    cg.add_define("USE_AUDIO_FLAC_SUPPORT", True)
    if CORE.using_esp_idf:
        esp32.add_idf_component(
            name="micro-opus",
            repo="https://github.com/esphome-libs/micro-opus.git",
            ref="v0.1.0",
        )

    # High performance networking is automatically configured via network.require_high_performance_networking()
    # called in CONFIG_SCHEMA validation. WiFi and lwip settings are applied by the wifi and network components.

    # Enable WebSocket support for the Sendspin HTTP server
    if CORE.using_esp_idf:
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
