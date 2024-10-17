from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_STATE
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
    CONF_BINARY_INPUT_ATTRIB_LIST,
    CONF_BINARY_INPUT_CLUSTER_LIST,
    CONF_BINARY_INPUT_EP,
    CONF_GROUPS_ATTRIB_LIST,
    CONF_IDENTIFY_ATTRIB_LIST,
    CONF_SCENES_ATTRIB_LIST,
    CONF_ZIGBEE_ID,
    BinaryAttrs,
    esphome_zb_ha_declare_binary_input_ep,
    zigbee_ns,
)

AUTO_LOAD = ["zigbee"]

ZigbeeBinarySensor = zigbee_ns.class_(
    "ZigbeeBinarySensor", binary_sensor.BinarySensor, cg.Component
)
CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(ZigbeeBinarySensor)
    .extend(
        {
            cv.GenerateID(CONF_BINARY_ATTRS): cv.declare_id(BinaryAttrs),
            cv.GenerateID(CONF_BINARY_INPUT_ATTRIB_LIST): cv.declare_id(
                cg.global_ns.namespace(
                    "ESPHOME_ZB_ZCL_DECLARE_BINARY_INPUT_ATTRIB_LIST"
                )
            ),
            cv.GenerateID(CONF_BINARY_INPUT_CLUSTER_LIST): cv.declare_id(
                cg.global_ns.namespace(
                    "ESPHOME_ZB_HA_DECLARE_BINARY_INPUT_CLUSTER_LIST"
                )
            ),
            cv.GenerateID(CONF_BINARY_INPUT_EP): cv.declare_id(
                esphome_zb_ha_declare_binary_input_ep
            ),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ZigbeeBaseSchema)
)


@coroutine_with_priority(50.0)
async def to_code(config):
    binary_attrs = zigbee_new_variable(config[CONF_BINARY_ATTRS])
    attr_list = zigbee_new_attr_list(
        config[CONF_BINARY_INPUT_ATTRIB_LIST],
        zigbee_assign(binary_attrs.out_of_service, 0),
        zigbee_assign(binary_attrs.present_value, 0),
        zigbee_assign(binary_attrs.status_flags, 0),
        zigbee_set_string(binary_attrs.description, config[CONF_NAME]),
    )

    cluster = zigbee_new_cluster_list(
        config[CONF_BINARY_INPUT_CLUSTER_LIST],
        attr_list,
        config[CONF_BASIC_ATTRIB_LIST_EXT],
        config[CONF_IDENTIFY_ATTRIB_LIST],
        config[CONF_GROUPS_ATTRIB_LIST],
        config[CONF_SCENES_ATTRIB_LIST],
    )

    ep = zigbee_register_ep(config[CONF_BINARY_INPUT_EP], cluster)

    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(bool)
        )
        cg.add(var.set_template(template_))

    cg.add(var.set_ep(ep))
    cg.add(var.set_cluster_attributes(binary_attrs))
    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))


@automation.register_action(
    "binary_sensor.zigbee.publish",
    binary_sensor.BinarySensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
        }
    ),
)
async def binary_sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))
    return var
