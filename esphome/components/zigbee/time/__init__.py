import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.types import ConfigType

from .. import consume_endpoint
from ..const_zephyr import CONF_ZIGBEE_ID, zigbee_ns
from ..zigbee_zephyr import (
    ZigbeeClusterDesc,
    ZigbeeComponent,
    get_slot_index,
    zigbee_new_attr_list,
    zigbee_new_cluster_list,
    zigbee_new_variable,
    zigbee_register_ep,
)

DEPENDENCIES = ["zigbee"]

ZigbeeTime = zigbee_ns.class_("ZigbeeTime", time_.RealTimeClock)

CONFIG_SCHEMA = cv.All(
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ZigbeeTime),
            cv.OnlyWith(CONF_ZIGBEE_ID, ["nrf52", "zigbee"]): cv.use_id(
                ZigbeeComponent
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("1s")),
    consume_endpoint,
)


async def to_code(config: ConfigType) -> None:
    CORE.add_job(_add_time, config)


async def _add_time(config: ConfigType) -> None:
    slot_index = get_slot_index()

    # Create unique names for this sensor's variables based on slot index
    prefix = f"zigbee_ep{slot_index + 1}"
    attrs_name = f"{prefix}_time_attrs"
    attr_list_name = f"{prefix}_time_attrib_list"
    cluster_list_name = f"{prefix}_cluster_list"
    ep_name = f"{prefix}_ep"

    # Create the binary attributes structure
    time_attrs = zigbee_new_variable(attrs_name, "zb_zcl_time_attrs_t")
    attr_list = zigbee_new_attr_list(
        attr_list_name,
        "ZB_ZCL_DECLARE_TIME_ATTR_LIST",
        str(time_attrs),
    )

    # Create cluster list and register endpoint
    cluster_list_name, clusters = zigbee_new_cluster_list(
        cluster_list_name,
        [
            ZigbeeClusterDesc("ZB_ZCL_CLUSTER_ID_TIME", attr_list),
            ZigbeeClusterDesc("ZB_ZCL_CLUSTER_ID_TIME"),
        ],
    )
    zigbee_register_ep(
        ep_name,
        cluster_list_name,
        0,
        clusters,
        slot_index,
        "ZB_HA_CUSTOM_ATTR_DEVICE_ID",
    )

    # Create the ZigbeeTime component
    var = cg.new_Pvariable(config[CONF_ID])
    await time_.register_time(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_endpoint(slot_index + 1))
    cg.add(var.set_cluster_attributes(time_attrs))
    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))
