import esphome.codegen as cg
from esphome.components import uart
from esphome.config_helpers import filter_source_files_from_platform
from esphome.const import PlatformFramework

nextion_ns = cg.esphome_ns.namespace("nextion")
Nextion = nextion_ns.class_("Nextion", cg.PollingComponent, uart.UARTDevice)
nextion_ref = Nextion.operator("ref")

CONF_NEXTION_ID = "nextion_id"
CONF_PUBLISH_STATE = "publish_state"
CONF_SEND_TO_NEXTION = "send_to_nextion"

DOMAIN = "nextion"
FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "nextion_upload_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "nextion_upload_arduino.cpp": {
            PlatformFramework.ESP8266_ARDUINO,
        },
    }
)

register_sensor_component = cg.slot_list(
    "NEXTION_SENSOR_COUNT", "register_sensor_component", DOMAIN
)
register_textsensor_component = cg.slot_list(
    "NEXTION_TEXT_SENSOR_COUNT", "register_textsensor_component", DOMAIN
)
register_switch_component = cg.slot_list(
    "NEXTION_SWITCH_COUNT", "register_switch_component", DOMAIN
)
register_touch_component = cg.slot_list(
    "NEXTION_TOUCH_COUNT", "register_touch_component", DOMAIN
)
register_binarysensor_component = cg.slot_list(
    "NEXTION_BINARY_SENSOR_COUNT", "register_binarysensor_component", DOMAIN
)
