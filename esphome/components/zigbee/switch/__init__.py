from esphome import automation
import esphome.codegen as cg
from esphome.components import output, switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT, CONF_STATE
from esphome.core import coroutine_with_priority

from .. import (
    ZigbeeBaseSchema,
    zigbee_assign,
    zigbee_new_attr_list,
    zigbee_new_cluster_list,
    zigbee_new_variable,
    zigbee_register_ep,
    zigbee_set_string,
)
from ..const import (
    CONF_BASIC_ATTRIB_LIST_EXT,
    CONF_BINARY_ATTRS,
    CONF_BINARY_OUTPUT_ATTRIB_LIST,
    CONF_BINARY_OUTPUT_CLUSTER_LIST,
    CONF_BINARY_OUTPUT_EP,
    CONF_GROUPS_ATTRIB_LIST,
    CONF_IDENTIFY_ATTRIB_LIST,
    CONF_SCENES_ATTRIB_LIST,
    CONF_ZIGBEE_ID,
    BinaryAttrs,
    esphome_zb_ha_declare_binary_output_ep,
    zigbee_ns,
)

AUTO_LOAD = ["zigbee"]

ZigbeeSwitch = zigbee_ns.class_("ZigbeeSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
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
            cv.GenerateID(CONF_BINARY_OUTPUT_CLUSTER_LIST): cv.declare_id(
                cg.global_ns.namespace(
                    "ESPHOME_ZB_HA_DECLARE_BINARY_OUTPUT_CLUSTER_LIST"
                )
            ),
            cv.GenerateID(CONF_BINARY_OUTPUT_EP): cv.declare_id(
                esphome_zb_ha_declare_binary_output_ep
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ZigbeeBaseSchema)
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

    cluster = zigbee_new_cluster_list(
        config[CONF_BINARY_OUTPUT_CLUSTER_LIST],
        attr_list,
        config[CONF_BASIC_ATTRIB_LIST_EXT],
        config[CONF_IDENTIFY_ATTRIB_LIST],
        config[CONF_GROUPS_ATTRIB_LIST],
        config[CONF_SCENES_ATTRIB_LIST],
    )

    ep = zigbee_register_ep(config[CONF_BINARY_OUTPUT_EP], cluster)

    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(output_))
    cg.add(var.set_ep(ep))
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
