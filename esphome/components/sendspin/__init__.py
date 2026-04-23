from dataclasses import dataclass

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32, network, psram, socket, wifi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TASK_STACK_IN_PSRAM
from esphome.core import CORE, ID
from esphome.cpp_generator import TemplateArgsType
from esphome.types import ConfigType

# mdns for autodiscovery
AUTO_LOAD = ["mdns"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["network"]
DOMAIN = "sendspin"

CONF_SENDSPIN_ID = "sendspin_id"

# Trailing underscore avoids clashing with sendspin-cpp's global `sendspin` namespace.
# Analysis tools strip the trailing underscore (same pattern as `template_`).
sendspin_ns = cg.esphome_ns.namespace("sendspin_")
SendspinHub = sendspin_ns.class_(
    "SendspinHub",
    cg.Component,
)


SendspinSwitchCommandAction = sendspin_ns.class_(
    "SendspinSwitchCommandAction",
    automation.Action,
    cg.Parented.template(SendspinHub),
)


@dataclass
class SendspinConfiguration:
    artwork_support: bool = False
    controller_support: bool = False
    metadata_support: bool = False
    player_support: bool = False
    visualizer_support: bool = False


def _get_data() -> SendspinConfiguration:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = SendspinConfiguration()
    return CORE.data[DOMAIN]


def request_artwork_support() -> None:
    """Request artwork role support for Sendspin."""
    _get_data().artwork_support = True


def request_controller_support() -> None:
    """Request controller role support for Sendspin."""
    _get_data().controller_support = True


def request_metadata_support() -> None:
    """Request metadata role support for Sendspin."""
    _get_data().metadata_support = True


def request_player_support() -> None:
    """Request player role support for Sendspin."""
    _get_data().player_support = True


def request_visualizer_support() -> None:
    """Request visualizer role support for Sendspin."""
    _get_data().visualizer_support = True


def _validate_task_stack_in_psram(value):
    value = cv.boolean(value)
    if value:
        return cv.requires_component(psram.DOMAIN)(value)
    return value


def _request_high_performance_networking(config: ConfigType) -> ConfigType:
    """Request high performance networking for Sendspin streaming.

    Also enables wake_loop_threadsafe support for fast defer() callbacks
    from background threads (WebSocket handler, image decoder).
    """
    network.require_high_performance_networking()
    # Socket consumption varies by mode:
    # - Server mode: 1 listening socket + 2 client connections (for handoff)
    # - Client mode: 1 outbound connection
    socket.consume_sockets(
        1, "sendspin_websocket_server", socket.SocketType.TCP_LISTEN
    )(config)
    socket.consume_sockets(2, "sendspin_websocket_server")(config)
    socket.consume_sockets(1, "sendspin_websocket_client")(config)

    wifi.enable_runtime_power_save_control()
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SendspinHub),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): _validate_task_stack_in_psram,
        }
    ),
    cv.only_on_esp32,
    _request_high_performance_networking,
)


def _request_controller_role(config: ConfigType) -> ConfigType:
    """Request the controller role for the sendspin.switch action."""
    request_controller_support()
    return config


SENDSPIN_SIMPLE_ACTION_SCHEMA = cv.All(
    automation.maybe_simple_id(
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(SendspinHub),
            }
        )
    ),
    _request_controller_role,
)


@automation.register_action(
    "sendspin.switch",
    SendspinSwitchCommandAction,
    SENDSPIN_SIMPLE_ACTION_SCHEMA,
    synchronous=True,
)
async def sendspin_switch_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        esp32.add_idf_sdkconfig_option(
            "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
        )

    # sendspin-cpp library
    esp32.add_idf_component(name="sendspin/sendspin-cpp", ref="0.3.0")

    cg.add_define("USE_SENDSPIN", True)  # for MDNS

    data = _get_data()

    # Configure Sendspin roles based on requested features (ESPHome internally via USE_SENDSPIN_*)
    # and disable building unused code paths in the sendspin-cpp library (IDF SDKConfig via CONFIG_SENDSPIN_ENABLE_*).
    if data.artwork_support:
        cg.add_define("USE_SENDSPIN_ARTWORK", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_ARTWORK", False)

    if data.controller_support:
        cg.add_define("USE_SENDSPIN_CONTROLLER", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_CONTROLLER", False)

    if data.metadata_support:
        cg.add_define("USE_SENDSPIN_METADATA", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_METADATA", False)

    if data.player_support:
        cg.add_define("USE_SENDSPIN_PLAYER", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_PLAYER", False)

    if data.visualizer_support:
        cg.add_define("USE_SENDSPIN_VISUALIZER", True)
    else:
        esp32.add_idf_sdkconfig_option("CONFIG_SENDSPIN_ENABLE_VISUALIZER", False)
