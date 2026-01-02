from esphome import automation, core
import esphome.codegen as cg
from esphome.components import socket, wifi
from esphome.components.udp import CONF_ON_RECEIVE
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CHANNEL,
    CONF_DATA,
    CONF_ENABLE_ON_BOOT,
    CONF_ID,
    CONF_ON_ERROR,
    CONF_TRIGGER_ID,
    CONF_WIFI,
)
from esphome.core import CORE, HexInt
from esphome.types import ConfigType

CODEOWNERS = ["@jesserockz"]
AUTO_LOAD = ["socket"]

byte_vector = cg.std_vector.template(cg.uint8)
peer_address_t = cg.std_ns.class_("array").template(cg.uint8, 6)

espnow_ns = cg.esphome_ns.namespace("espnow")
ESPNowComponent = espnow_ns.class_("ESPNowComponent", cg.Component)

# Handler interfaces that other components can use to register callbacks
ESPNowReceivedPacketHandler = espnow_ns.class_("ESPNowReceivedPacketHandler")
ESPNowUnknownPeerHandler = espnow_ns.class_("ESPNowUnknownPeerHandler")
ESPNowBroadcastedHandler = espnow_ns.class_("ESPNowBroadcastedHandler")

ESPNowRecvInfo = espnow_ns.class_("ESPNowRecvInfo")
ESPNowRecvInfoConstRef = ESPNowRecvInfo.operator("const").operator("ref")

SendAction = espnow_ns.class_("SendAction", automation.Action)
SetChannelAction = espnow_ns.class_("SetChannelAction", automation.Action)
AddPeerAction = espnow_ns.class_("AddPeerAction", automation.Action)
DeletePeerAction = espnow_ns.class_("DeletePeerAction", automation.Action)

ESPNowHandlerTrigger = automation.Trigger.template(
    ESPNowRecvInfoConstRef,
    cg.uint8.operator("const").operator("ptr"),
    cg.uint8,
)

OnUnknownPeerTrigger = espnow_ns.class_(
    "OnUnknownPeerTrigger", ESPNowHandlerTrigger, ESPNowUnknownPeerHandler
)
OnReceiveTrigger = espnow_ns.class_(
    "OnReceiveTrigger", ESPNowHandlerTrigger, ESPNowReceivedPacketHandler
)
OnBroadcastedTrigger = espnow_ns.class_(
    "OnBroadcastedTrigger", ESPNowHandlerTrigger, ESPNowBroadcastedHandler
)


CONF_AUTO_ADD_PEER = "auto_add_peer"
CONF_PEERS = "peers"
CONF_ON_SENT = "on_sent"
CONF_ON_UNKNOWN_PEER = "on_unknown_peer"
CONF_ON_BROADCAST = "on_broadcast"
CONF_CONTINUE_ON_ERROR = "continue_on_error"
CONF_WAIT_FOR_SENT = "wait_for_sent"

MAX_ESPNOW_PACKET_SIZE = 250  # Maximum size of the payload in bytes


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPNowComponent),
            cv.OnlyWithout(CONF_CHANNEL, CONF_WIFI): wifi.validate_channel,
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_AUTO_ADD_PEER, default=False): cv.boolean,
            cv.Optional(CONF_PEERS): cv.ensure_list(cv.mac_address),
            cv.Optional(CONF_ON_UNKNOWN_PEER): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnUnknownPeerTrigger),
                },
                single=True,
            ),
            cv.Optional(CONF_ON_RECEIVE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnReceiveTrigger),
                    cv.Optional(CONF_ADDRESS): cv.mac_address,
                }
            ),
            cv.Optional(CONF_ON_BROADCAST): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnBroadcastedTrigger),
                    cv.Optional(CONF_ADDRESS): cv.mac_address,
                }
            ),
        },
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def _trigger_to_code(config):
    if address := config.get(CONF_ADDRESS):
        address = address.parts
    trigger = cg.new_Pvariable(config[CONF_TRIGGER_ID], address)
    await automation.build_automation(
        trigger,
        [
            (ESPNowRecvInfoConstRef, "info"),
            (cg.uint8.operator("const").operator("ptr"), "data"),
            (cg.uint8, "size"),
        ],
        config,
    )
    return trigger


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CORE.using_arduino:
        cg.add_library("WiFi", None)

    # ESP-NOW uses wake_loop_threadsafe() to wake the main loop from ESP-NOW callbacks
    # This enables low-latency event processing instead of waiting for select() timeout
    socket.require_wake_loop_threadsafe()

    cg.add_define("USE_ESPNOW")
    if wifi_channel := config.get(CONF_CHANNEL):
        cg.add(var.set_wifi_channel(wifi_channel))

    cg.add(var.set_auto_add_peer(config[CONF_AUTO_ADD_PEER]))

    for peer in config.get(CONF_PEERS, []):
        cg.add(var.add_peer(peer.parts))

    if on_receive := config.get(CONF_ON_UNKNOWN_PEER):
        trigger = await _trigger_to_code(on_receive)
        cg.add(var.register_unknown_peer_handler(trigger))

    for on_receive in config.get(CONF_ON_RECEIVE, []):
        trigger = await _trigger_to_code(on_receive)
        cg.add(var.register_received_handler(trigger))

    for on_receive in config.get(CONF_ON_BROADCAST, []):
        trigger = await _trigger_to_code(on_receive)
        cg.add(var.register_broadcasted_handler(trigger))


# ========================================== A C T I O N S ================================================


def validate_peer(value):
    if isinstance(value, cv.Lambda):
        return cv.returning_lambda(value)
    return cv.mac_address(value)


def _validate_raw_data(value):
    if isinstance(value, str):
        if len(value) >= MAX_ESPNOW_PACKET_SIZE:
            raise cv.Invalid(
                f"'{CONF_DATA}' must be less than {MAX_ESPNOW_PACKET_SIZE} characters long, got {len(value)}"
            )
        return value
    if isinstance(value, list):
        if len(value) > MAX_ESPNOW_PACKET_SIZE:
            raise cv.Invalid(
                f"'{CONF_DATA}' must be less than {MAX_ESPNOW_PACKET_SIZE} bytes long, got {len(value)}"
            )
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        f"'{CONF_DATA}' must either be a string wrapped in quotes or a list of bytes"
    )


async def register_peer(var, config, args):
    peer = config[CONF_ADDRESS]
    if isinstance(peer, core.MACAddress):
        peer = [HexInt(p) for p in peer.parts]

    template_ = await cg.templatable(peer, args, peer_address_t, peer_address_t)
    cg.add(var.set_address(template_))


PEER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(ESPNowComponent),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.mac_address),
    }
)

SEND_SCHEMA = PEER_SCHEMA.extend(
    {
        cv.Required(CONF_DATA): cv.templatable(_validate_raw_data),
        cv.Optional(CONF_ON_SENT): automation.validate_action_list,
        cv.Optional(CONF_ON_ERROR): automation.validate_action_list,
        cv.Optional(CONF_WAIT_FOR_SENT, default=True): cv.boolean,
        cv.Optional(CONF_CONTINUE_ON_ERROR, default=True): cv.boolean,
    }
)


def _validate_send_action(config):
    if not config[CONF_WAIT_FOR_SENT] and not config[CONF_CONTINUE_ON_ERROR]:
        raise cv.Invalid(
            f"'{CONF_CONTINUE_ON_ERROR}' cannot be false if '{CONF_WAIT_FOR_SENT}' is false as the automation will not wait for the failed result.",
            path=[CONF_CONTINUE_ON_ERROR],
        )
    return config


SEND_SCHEMA.add_extra(_validate_send_action)


@automation.register_action(
    "espnow.send",
    SendAction,
    SEND_SCHEMA,
)
@automation.register_action(
    "espnow.broadcast",
    SendAction,
    cv.maybe_simple_value(
        SEND_SCHEMA.extend(
            {
                cv.Optional(CONF_ADDRESS, default="FF:FF:FF:FF:FF:FF"): cv.mac_address,
            }
        ),
        key=CONF_DATA,
    ),
)
async def send_action(
    config: ConfigType,
    action_id: core.ID,
    template_arg: cg.TemplateArguments,
    args: list[tuple],
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    await register_peer(var, config, args)

    data = config.get(CONF_DATA, [])
    if isinstance(data, str):
        data = [cg.RawExpression(f"'{c}'") for c in data]
    templ = await cg.templatable(data, args, byte_vector, byte_vector)
    cg.add(var.set_data(templ))

    cg.add(var.set_wait_for_sent(config[CONF_WAIT_FOR_SENT]))
    cg.add(var.set_continue_on_error(config[CONF_CONTINUE_ON_ERROR]))

    if on_sent_config := config.get(CONF_ON_SENT):
        actions = await automation.build_action_list(on_sent_config, template_arg, args)
        cg.add(var.add_on_sent(actions))
    if on_error_config := config.get(CONF_ON_ERROR):
        actions = await automation.build_action_list(
            on_error_config, template_arg, args
        )
        cg.add(var.add_on_error(actions))
    return var


@automation.register_action(
    "espnow.peer.add",
    AddPeerAction,
    cv.maybe_simple_value(
        PEER_SCHEMA,
        key=CONF_ADDRESS,
    ),
)
@automation.register_action(
    "espnow.peer.delete",
    DeletePeerAction,
    cv.maybe_simple_value(
        PEER_SCHEMA,
        key=CONF_ADDRESS,
    ),
)
async def peer_action(
    config: ConfigType,
    action_id: core.ID,
    template_arg: cg.TemplateArguments,
    args: list[tuple],
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    await register_peer(var, config, args)

    return var


@automation.register_action(
    "espnow.set_channel",
    SetChannelAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_CHANNEL): cv.templatable(wifi.validate_channel),
        },
        key=CONF_CHANNEL,
    ),
)
async def channel_action(
    config: ConfigType,
    action_id: core.ID,
    template_arg: cg.TemplateArguments,
    args: list[tuple],
):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)
    cg.add(var.set_channel(template_))
    return var
