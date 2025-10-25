from esphome import automation
import esphome.codegen as cg
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG, Section
from esphome.components.zephyr import zephyr_add_pm_static
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

from .const_zephyr import (
    CONF_BASIC_ATTRIB_LIST_EXT,
    CONF_BASIC_ATTRS_EXT,
    CONF_BINARY_ATTRS,
    CONF_BINARY_INPUT_ATTRIB_LIST,
    CONF_CLUSTER_LIST,
    CONF_EP,
    CONF_GROUPS_ATTRIB_LIST,
    CONF_GROUPS_ATTRS,
    CONF_IDENTIFY_ATTRIB_LIST,
    CONF_IDENTIFY_ATTRS,
    CONF_MAX_EP_NUMBER,
    CONF_ON_JOIN,
    CONF_SCENES_ATTRIB_LIST,
    CONF_SCENES_ATTRS,
    CONF_WIPE_ON_BOOT,
    CONF_ZIGBEE_ID,
    ESPHOME_ZB_HA_DECLARE_EP,
    KEY_EP_NUMBER,
    KEY_ZIGBEE,
    ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT,
    ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST,
    ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST,
    ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST,
    BinaryAttrs,
    Zigbee,
    zb_zcl_basic_attrs_ext_t,
    zb_zcl_groups_attrs_t,
    zb_zcl_identify_attrs_t,
    zb_zcl_scenes_attrs_t,
    zigbee_ns,
)

CODEOWNERS = ["@tomaszduda23"]


def zigbee_set_core_data(config):
    if CORE.data[KEY_ZEPHYR][KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        zephyr_add_pm_static(
            [Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary")]
        )

    return config


ZigbeeBaseSchema = cv.Schema(
    {
        cv.OnlyWith(CONF_ZIGBEE_ID, "zigbee"): cv.use_id(Zigbee),
        cv.OnlyWith(CONF_BASIC_ATTRIB_LIST_EXT, "zigbee"): cv.use_id(
            ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT
        ),
        cv.OnlyWith(CONF_IDENTIFY_ATTRIB_LIST, "zigbee"): cv.use_id(
            ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST
        ),
        cv.OnlyWith(CONF_GROUPS_ATTRIB_LIST, "zigbee"): cv.use_id(
            ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST
        ),
        cv.OnlyWith(CONF_SCENES_ATTRIB_LIST, "zigbee"): cv.use_id(
            ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST
        ),
        cv.OnlyWith(CONF_EP, "zigbee"): cv.declare_id(ESPHOME_ZB_HA_DECLARE_EP),
        cv.OnlyWith(CONF_CLUSTER_LIST, "zigbee"): cv.declare_id(
            cg.global_ns.namespace("zb_zcl_cluster_desc_t")
        ),
    },
)

CONF_ZIGBEE_BINARY_SENSOR = "zigbee_binary_sensor"
ZigbeeBinarySensor = zigbee_ns.class_("ZigbeeBinarySensor", cg.Component)

ZIGBEE_BINARY_SENSOR_SCHEMA = cv.Schema(
    {
        cv.OnlyWith(CONF_ZIGBEE_BINARY_SENSOR, "zigbee"): cv.declare_id(
            ZigbeeBinarySensor
        ),
        cv.OnlyWith(CONF_BINARY_ATTRS, "zigbee"): cv.declare_id(BinaryAttrs),
        cv.OnlyWith(CONF_BINARY_INPUT_ATTRIB_LIST, "zigbee"): cv.declare_id(
            cg.global_ns.namespace("ESPHOME_ZB_ZCL_DECLARE_BINARY_INPUT_ATTRIB_LIST")
        ),
    }
).extend(ZigbeeBaseSchema)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(Zigbee),
            cv.GenerateID(CONF_BASIC_ATTRS_EXT): cv.declare_id(
                zb_zcl_basic_attrs_ext_t
            ),
            cv.GenerateID(CONF_BASIC_ATTRIB_LIST_EXT): cv.declare_id(
                ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT
            ),
            cv.GenerateID(CONF_IDENTIFY_ATTRS): cv.declare_id(zb_zcl_identify_attrs_t),
            cv.GenerateID(CONF_IDENTIFY_ATTRIB_LIST): cv.declare_id(
                ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST
            ),
            cv.GenerateID(CONF_GROUPS_ATTRS): cv.declare_id(zb_zcl_groups_attrs_t),
            cv.GenerateID(CONF_GROUPS_ATTRIB_LIST): cv.declare_id(
                ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST
            ),
            cv.GenerateID(CONF_SCENES_ATTRS): cv.declare_id(zb_zcl_scenes_attrs_t),
            cv.GenerateID(CONF_SCENES_ATTRIB_LIST): cv.declare_id(
                ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST
            ),
            cv.Optional(CONF_ON_JOIN): automation.validate_automation(single=True),
            cv.Optional(CONF_WIPE_ON_BOOT, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    zigbee_set_core_data,
)


def validate_number_of_ep(config):
    if KEY_ZIGBEE not in CORE.data:
        raise cv.Invalid("At least one end point shall be defined")
    count = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    if count > CONF_MAX_EP_NUMBER:
        raise cv.Invalid(f"Maximum number of end points is {CONF_MAX_EP_NUMBER}")
    if count == 0:
        raise cv.Invalid("At least one zigbee device need to be included")


# FINAL_VALIDATE_SCHEMA = cv.All(
#     validate_number_of_ep,
# )


async def to_code(config):
    cg.add_define("USE_ZIGBEE")


async def setup_zigbee_binary_sensor(entity, config):
    if not config.get(CONF_ZIGBEE_ID):
        return
