from esphome import automation
import esphome.codegen as cg

# WifiNow global
wifi_now_ns = cg.esphome_ns.namespace("wifi_now")

# WifiNow simple types
payload_t = wifi_now_ns.namespace("payload_t")

# WifiNow component
Component = wifi_now_ns.class_("WifiNowComponent", cg.Component)
ReceiveTrigger = wifi_now_ns.class_(
    "WifiNowReceiveTrigger", automation.Trigger, cg.Component
)
InjectAction = wifi_now_ns.class_("WifiNowInjectAction", automation.Action)

# WifiNow peer
Peer = wifi_now_ns.class_("WifiNowPeer")

# WifiNow automation
TemplatePayloadGetter = wifi_now_ns.class_("WifiNowTemplatePayloadGetter")
TemplatePayloadSetter = wifi_now_ns.class_("WifiNowTemplatePayloadSetter")
PayloadPayloadGetter = wifi_now_ns.class_("WifiNowPayloadPayloadGetter")
PayloadPayloadSetter = wifi_now_ns.class_("WifiNowPayloadPayloadSetter")

SendAction = wifi_now_ns.class_("WifiNowSendAction", automation.Action)
TerminalAction = wifi_now_ns.class_("WifiNowTerminalAction", automation.Action)

BinarySensorEvent = wifi_now_ns.enum("WifiNowBinarySensorEvent")
BINARY_SENSOR_EVENTS = {
    "on": BinarySensorEvent.PRESS,
    "off": BinarySensorEvent.RELEASE,
    "click": BinarySensorEvent.CLICK,
    "double_click": BinarySensorEvent.DOUBLE_CLICK,
    "multi_click_1": BinarySensorEvent.MULTI_CLICK1,
    "multi_click_2": BinarySensorEvent.MULTI_CLICK2,
    "multi_click_3": BinarySensorEvent.MULTI_CLICK3,
    "multi_click_4": BinarySensorEvent.MULTI_CLICK4,
    "multi_click_5": BinarySensorEvent.MULTI_CLICK5,
    "multi_click_6": BinarySensorEvent.MULTI_CLICK6,
    "multi_click_7": BinarySensorEvent.MULTI_CLICK7,
    "multi_click_8": BinarySensorEvent.MULTI_CLICK8,
    "multi_click_9": BinarySensorEvent.MULTI_CLICK9,
    "multi_click_10": BinarySensorEvent.MULTI_CLICK10,
    "multi_click_11": BinarySensorEvent.MULTI_CLICK11,
    "multi_click_12": BinarySensorEvent.MULTI_CLICK12,
    "multi_click_13": BinarySensorEvent.MULTI_CLICK13,
    "multi_click_14": BinarySensorEvent.MULTI_CLICK14,
    "multi_click_15": BinarySensorEvent.MULTI_CLICK15,
    "multi_click_16": BinarySensorEvent.MULTI_CLICK16,
    "multi_click_17": BinarySensorEvent.MULTI_CLICK17,
    "multi_click_18": BinarySensorEvent.MULTI_CLICK18,
    "multi_click_19": BinarySensorEvent.MULTI_CLICK19,
    "multi_click_20": BinarySensorEvent.MULTI_CLICK20,
}
