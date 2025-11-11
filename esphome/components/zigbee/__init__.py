from collections.abc import MutableMapping
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components.nrf52.boards import BOOTLOADER_CONFIG, Section
from esphome.components.zephyr import zephyr_add_pm_static, zephyr_data
from esphome.components.zephyr.const import KEY_BOOTLOADER
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
    CONF_ZIGBEE_BINARY_SENSOR,
    CONF_ZIGBEE_ID,
    ESPHOME_ZB_HA_DECLARE_EP,
    KEY_EP_NUMBER,
    KEY_ZIGBEE,
    ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT,
    ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST,
    ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST,
    ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST,
    BinaryAttrs,
    ZigbeeComponent,
    zb_zcl_basic_attrs_ext_t,
    zb_zcl_groups_attrs_t,
    zb_zcl_identify_attrs_t,
    zb_zcl_scenes_attrs_t,
    zigbee_ns,
)

CODEOWNERS = ["@tomaszduda23"]


def zigbee_set_core_data(config):
    if zephyr_data()[KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        zephyr_add_pm_static(
            [Section("empty_after_zboss_offset", 0xF4000, 0xC000, "flash_primary")]
        )

    return config


def consume_ep_slots(config: MutableMapping) -> MutableMapping:
    data: dict[str, Any] = CORE.data.setdefault(KEY_ZIGBEE, {})
    slots: list[str] = data.setdefault(KEY_EP_NUMBER, [])
    slots.extend([""])
    return config


ZigbeeBaseSchema = cv.Schema(
    {
        cv.OnlyWith(CONF_ZIGBEE_ID, ["nrf52", "zigbee"]): cv.use_id(ZigbeeComponent),
        cv.OnlyWith(CONF_BASIC_ATTRIB_LIST_EXT, ["nrf52", "zigbee"]): cv.use_id(
            ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT
        ),
        cv.OnlyWith(CONF_IDENTIFY_ATTRIB_LIST, ["nrf52", "zigbee"]): cv.use_id(
            ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST
        ),
        cv.OnlyWith(CONF_GROUPS_ATTRIB_LIST, ["nrf52", "zigbee"]): cv.use_id(
            ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST
        ),
        cv.OnlyWith(CONF_SCENES_ATTRIB_LIST, ["nrf52", "zigbee"]): cv.use_id(
            ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST
        ),
        cv.OnlyWith(CONF_EP, ["nrf52", "zigbee"]): cv.declare_id(
            ESPHOME_ZB_HA_DECLARE_EP
        ),
        cv.OnlyWith(CONF_CLUSTER_LIST, ["nrf52", "zigbee"]): cv.declare_id(
            cg.global_ns.namespace("zb_zcl_cluster_desc_t")
        ),
    },
)

ZigbeeBinarySensor = zigbee_ns.class_("ZigbeeBinarySensor", cg.Component)

ZIGBEE_BINARY_SENSOR_SCHEMA = cv.Schema(
    {
        cv.OnlyWith(CONF_ZIGBEE_BINARY_SENSOR, ["nrf52", "zigbee"]): cv.All(
            cv.declare_id(ZigbeeBinarySensor), consume_ep_slots
        ),
        cv.OnlyWith(CONF_BINARY_ATTRS, ["nrf52", "zigbee"]): cv.declare_id(BinaryAttrs),
        cv.OnlyWith(CONF_BINARY_INPUT_ATTRIB_LIST, ["nrf52", "zigbee"]): cv.declare_id(
            cg.global_ns.namespace("ESPHOME_ZB_ZCL_DECLARE_BINARY_INPUT_ATTRIB_LIST")
        ),
    }
).extend(ZigbeeBaseSchema)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(ZigbeeComponent),
            cv.OnlyWith(CONF_BASIC_ATTRS_EXT, "nrf52"): cv.declare_id(
                zb_zcl_basic_attrs_ext_t
            ),
            cv.OnlyWith(CONF_BASIC_ATTRIB_LIST_EXT, "nrf52"): cv.declare_id(
                ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT
            ),
            cv.OnlyWith(CONF_IDENTIFY_ATTRS, "nrf52"): cv.declare_id(
                zb_zcl_identify_attrs_t
            ),
            cv.OnlyWith(CONF_IDENTIFY_ATTRIB_LIST, "nrf52"): cv.declare_id(
                ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST
            ),
            cv.OnlyWith(CONF_GROUPS_ATTRS, "nrf52"): cv.declare_id(
                zb_zcl_groups_attrs_t
            ),
            cv.OnlyWith(CONF_GROUPS_ATTRIB_LIST, "nrf52"): cv.declare_id(
                ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST
            ),
            cv.OnlyWith(CONF_SCENES_ATTRS, "nrf52"): cv.declare_id(
                zb_zcl_scenes_attrs_t
            ),
            cv.OnlyWith(CONF_SCENES_ATTRIB_LIST, "nrf52"): cv.declare_id(
                ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST
            ),
            cv.Optional(CONF_ON_JOIN): automation.validate_automation(single=True),
            cv.Optional(CONF_WIPE_ON_BOOT, default=False): cv.All(
                cv.boolean,
                cv.requires_component("nrf52"),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    zigbee_set_core_data,
    cv.only_with_framework("zephyr"),
)


def validate_number_of_ep(config):
    if KEY_ZIGBEE not in CORE.data:
        raise cv.Invalid("At least one zigbee device need to be included")
    count = len(CORE.data[KEY_ZIGBEE][KEY_EP_NUMBER])
    if count > CONF_MAX_EP_NUMBER:
        raise cv.Invalid(f"Maximum number of end points is {CONF_MAX_EP_NUMBER}")


FINAL_VALIDATE_SCHEMA = cv.All(
    validate_number_of_ep,
)


async def to_code(config):
    cg.add_define("USE_ZIGBEE")
    if CORE.is_nrf52:
        from .zigbee_zephyr import zephyr_to_code

        await zephyr_to_code(config)


async def setup_zigbee_binary_sensor(entity, config):
    if not config.get(CONF_ZIGBEE_ID):
        return
    if CORE.is_nrf52:
        from .zigbee_zephyr import zephyr_setup_zigbee_binary_sensor

        await zephyr_setup_zigbee_binary_sensor(entity, config)


FactoryResetAction = zigbee_ns.class_("FactoryResetAction", automation.Action)


@automation.register_action("zigbee.factory_reset", FactoryResetAction, cv.Schema({}))
async def zigbee_factory_reset_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)
