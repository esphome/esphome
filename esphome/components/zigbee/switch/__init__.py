from esphome import automation
import esphome.codegen as cg
from esphome.components import output, switch
from esphome.components.zigbee import (
    KEY_EP_NUMBER,
    consume_ep_slots,
    zigbee_register_ep,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT, CONF_STATE
from esphome.core import coroutine_with_priority

from .. import (
    ZigbeeBaseSchema,
    ZigbeeClusterDesc,
    zigbee_assign,
    zigbee_new_attr_list,
    zigbee_new_cluster_list,
    zigbee_new_variable,
    zigbee_set_string,
)
from ..const import (
    CONF_BINARY_ATTRS,
    CONF_BINARY_OUTPUT_ATTRIB_LIST,
    CONF_ZIGBEE_ID,
    ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT,
    BinaryAttrs,
    zigbee_ns,
)

AUTO_LOAD = ["zigbee"]

ZigbeeSwitch = zigbee_ns.class_("ZigbeeSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = cv.All(
    switch.switch_schema(ZigbeeSwitch)
    .extend(
        {
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.GenerateID(CONF_BINARY_ATTRS): cv.declare_id(BinaryAttrs),
            cv.GenerateID(CONF_BINARY_OUTPUT_ATTRIB_LIST): cv.declare_id(
                cg.global_ns.namespace(
                    "ESPHOME_ZB_ZCL_DECLARE_BINARY_OUTPUT_ATTRIB_LIST"
                )
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ZigbeeBaseSchema),
    consume_ep_slots,
)


@coroutine_with_priority(50.0)
async def to_code(config):
    binary_attrs = zigbee_new_variable(config[CONF_BINARY_ATTRS])
    attr_list = zigbee_new_attr_list(
        config[CONF_BINARY_OUTPUT_ATTRIB_LIST],
        zigbee_assign(binary_attrs.out_of_service, 0),
        zigbee_assign(binary_attrs.present_value, 0),
        zigbee_assign(binary_attrs.status_flags, 0),
        zigbee_set_string(binary_attrs.description, config[CONF_NAME]),
    )

    cluster_id, clusters = zigbee_new_cluster_list(
        config, [ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, attr_list)]
    )

    zigbee_register_ep(config, cluster_id, 2, clusters)

    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(output_))
    cg.add(var.set_ep(config[KEY_EP_NUMBER]))
    cg.add(var.set_cluster_attributes(binary_attrs))
    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))


@automation.register_action(
    "switch.zigbee.publish",
    switch.SwitchPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(switch.Switch),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
        }
    ),
)
async def switch_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))
    return var
