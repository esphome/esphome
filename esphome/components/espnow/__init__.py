from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID, CONF_MAC_ADDRESS, CONF_TRIGGER_ID
from esphome.core import CORE

CODEOWNERS = ["@nielsnl68", "@jesserockz"]

espnow_ns = cg.esphome_ns.namespace("espnow")
ESPNowComponent = espnow_ns.class_("ESPNowComponent", cg.Component)
ESPNowListener = espnow_ns.class_("ESPNowListener")

ESPNowPackage = espnow_ns.class_("ESPNowPackage")
ESPNowPackagePtrConst = ESPNowPackage.operator("ptr")  # .operator("const")

ESPNowInterface = espnow_ns.class_(
    "ESPNowInterface", cg.Component, cg.Parented.template(ESPNowComponent)
)

ESPNowSendTrigger = espnow_ns.class_("ESPNowSendTrigger", automation.Trigger.template())
ESPNowReceiveTrigger = espnow_ns.class_(
    "ESPNowReceiveTrigger", automation.Trigger.template()
)
ESPNowNewPeerTrigger = espnow_ns.class_(
    "ESPNowNewPeerTrigger", automation.Trigger.template()
)

SendAction = espnow_ns.class_("SendAction", automation.Action)
NewPeerAction = espnow_ns.class_("NewPeerAction", automation.Action)
DelPeerAction = espnow_ns.class_("DelPeerAction", automation.Action)

CONF_ESPNOW = "espnow"
CONF_ON_PACKAGE_RECEIVED = "on_package_received"
CONF_ON_PACKAGE_SEND = "on_package_send"
CONF_ON_NEW_PEER = "on_new_peer"
CONF_WIFI_CHANNEL = "wifi_channel"
CONF_PEERS = "peers"
CONF_AUTO_ADD_PEER = "auto_add_peer"


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESPNowComponent),
        cv.Optional(CONF_WIFI_CHANNEL, default=0): cv.int_range(0, 14),
        cv.Optional(CONF_AUTO_ADD_PEER, default=False): cv.boolean,
        cv.Optional(CONF_ON_PACKAGE_RECEIVED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowReceiveTrigger),
            }
        ),
        cv.Optional(CONF_ON_PACKAGE_SEND): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowSendTrigger),
            }
        ),
        cv.Optional(CONF_ON_NEW_PEER): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPNowNewPeerTrigger),
            }
        ),
        cv.Optional(CONF_PEERS): cv.ensure_list(cv.mac_address),
    },
    cv.only_on_esp32,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CORE.is_esp8266:
        cg.add_library("ESP8266WiFi", None)
    elif CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("WiFi", None)
    elif CORE.is_rp2040:
        cg.add_library("WiFi", None)

    cg.add_define("USE_ESPNOW")

    cg.add(var.set_wifi_channel(config[CONF_WIFI_CHANNEL]))
    cg.add(var.set_auto_add_peer(config[CONF_AUTO_ADD_PEER]))

    for conf in config.get(CONF_ON_PACKAGE_SEND, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(ESPNowPackagePtrConst, "it")], conf
        )

    for conf in config.get(CONF_ON_PACKAGE_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(ESPNowPackagePtrConst, "it")], conf
        )

    for conf in config.get(CONF_ON_NEW_PEER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(ESPNowPackagePtrConst, "it")], conf
        )

    for conf in config.get(CONF_PEERS, []):
        cg.add(var.add_peer(conf.as_hex))


@automation.register_action(
    "espnow.send",
    SendAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Optional(CONF_MAC_ADDRESS): cv.templatable(cv.mac_address),
            cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
        },
        key=CONF_DATA,
    ),
)
async def send_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if CONF_MAC_ADDRESS in config:
        template_ = await cg.templatable(
            config[CONF_MAC_ADDRESS].as_hex, args, cg.uint64
        )
        cg.add(var.set_mac(template_))

    data = config.get(CONF_DATA, [])
    if isinstance(data, bytes):
        data = list(data)

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    return var


@automation.register_action(
    "espnow.new.peer",
    NewPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_MAC_ADDRESS): cv.templatable(cv.mac_address),
        },
        key=CONF_MAC_ADDRESS,
    ),
)
async def new_peer_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    template_ = await cg.templatable(config[CONF_MAC_ADDRESS].as_hex, args, cg.uint64)
    cg.add(var.set_mac(template_))
    return var


@automation.register_action(
    "espnow.del.peer",
    DelPeerAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(ESPNowComponent),
            cv.Required(CONF_MAC_ADDRESS): cv.templatable(cv.mac_address),
        },
        key=CONF_MAC_ADDRESS,
    ),
)
async def del_peer_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    template_ = await cg.templatable(config[CONF_MAC_ADDRESS].as_hex, args, cg.uint64)
    cg.add(var.set_mac(template_))
    return var
