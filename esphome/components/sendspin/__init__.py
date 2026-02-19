"""Sendspin Media Player Setup."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, esp32, network, socket, wifi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_TASK_STACK_IN_PSRAM,
    PLATFORM_ESP32,
)

# json handles server messages, mdns for autodiscovering, media player for stream commands (for now is autoloaded), psram for memory
AUTO_LOAD = ["audio", "json", "mdns", "media_player", "psram"]
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

SendSwitchCommandAction = sendspin_ns.class_(
    "SendSwitchCommandAction",
    automation.Action,
    cg.Parented.template(SendspinHub),
)


def _request_high_performance_networking(config):
    """Request high performance networking for Sendspin streaming.

    Also enables wake_loop_threadsafe support for fast defer() callbacks
    from background threads (WebSocket handler, image decoder).
    """
    network.require_high_performance_networking()
    # Socket consumption varies by mode:
    # - Server mode: 1 listening socket + 2 client connections (for handoff)
    # - Client mode: 1 outbound connection
    socket.consume_sockets(3, "sendspin_websocket_server")(config)
    socket.consume_sockets(1, "sendspin_websocket_client")(config)

    wifi.enable_runtime_power_save_control()  # TODO: Is this safe if wifi isn't configured?
    return config


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SendspinHub),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM, default=False): cv.boolean,
            cv.Optional(CONF_KALMAN_PROCESS_ERROR, default=0.01): cv.float_,
            cv.Optional(CONF_KALMAN_FORGET_FACTOR, default=1.001): cv.float_,
            cv.Optional(CONF_BUFFER_SIZE): cv.invalid(
                f"The {CONF_BUFFER_SIZE} option is now set as an option in the Sendspin media_source configuration"
            ),
        }
    ),
    cv.only_on([PLATFORM_ESP32]),
    _request_high_performance_networking,
)


def _final_validate_codecs(config):
    audio.request_flac_support()
    audio.request_opus_support()
    return config


FINAL_VALIDATE_SCHEMA = _final_validate_codecs


async def to_code(config):
    socket.require_wake_loop_threadsafe()

    cg.add_define("USE_SENDSPIN", True)  # for MDNS

    # Client mode - enable ESP-IDF WebSocket client
    esp32.add_idf_component(name="espressif/esp_websocket_client", ref="1.6.1")
    # Server mode - enable HTTP server with WebSocket support
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


SENDSPIN_SWITCH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SendspinHub),
    }
)


@automation.register_action(
    "sendspin.switch", SendSwitchCommandAction, SENDSPIN_SWITCH_ACTION_SCHEMA
)
async def sendspin_switch_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
