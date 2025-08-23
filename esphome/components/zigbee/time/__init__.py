import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority

from .. import (
    KEY_EP_NUMBER,
    ZigbeeBaseSchema,
    ZigbeeClusterDesc,
    consume_ep_slots,
    zigbee_new_attr_list,
    zigbee_new_cluster_list,
    zigbee_new_variable,
    zigbee_register_ep,
)
from ..const import (
    CONF_TIME_ATTRIB_LIST,
    CONF_TIME_ATTRS,
    CONF_ZIGBEE_ID,
    ZB_ZCL_CLUSTER_ID_TIME,
    zigbee_ns,
)

AUTO_LOAD = ["zigbee"]

ZigbeeTime = zigbee_ns.class_("ZigbeeTime", time_.RealTimeClock)

CONFIG_SCHEMA = cv.All(
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ZigbeeTime),
            cv.GenerateID(CONF_TIME_ATTRS): cv.declare_id(
                cg.global_ns.struct("zb_zcl_time_attrs_t")
            ),
            cv.GenerateID(CONF_TIME_ATTRIB_LIST): cv.declare_id(
                cg.global_ns.namespace("ZB_ZCL_DECLARE_TIME_ATTR_LIST")
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ZigbeeBaseSchema)
    .extend(cv.polling_component_schema("1s")),
    consume_ep_slots,
)


@coroutine_with_priority(50.0)
async def to_code(config):
    time_attrs = zigbee_new_variable(config[CONF_TIME_ATTRS])
    attr_list = zigbee_new_attr_list(
        config[CONF_TIME_ATTRIB_LIST],
        time_attrs,
    )
    cluster_id, clusters = zigbee_new_cluster_list(
        config,
        [
            ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_TIME, attr_list),
            ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_TIME),
        ],
    )

    zigbee_register_ep(config, cluster_id, 0, clusters)

    var = cg.new_Pvariable(config[CONF_ID])
    await time_.register_time(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_ep(config[KEY_EP_NUMBER]))
    cg.add(var.set_cluster_attributes(time_attrs))
    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))
