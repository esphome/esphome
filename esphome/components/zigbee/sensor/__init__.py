from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_STATE
from esphome.core import coroutine_with_priority

from .. import (
    KEY_EP_NUMBER,
    ZigbeeBaseSchema,
    ZigbeeClusterDesc,
    consume_ep_slots,
    zigbee_assign,
    zigbee_new_attr_list,
    zigbee_new_cluster_list,
    zigbee_new_variable,
    zigbee_register_ep,
    zigbee_set_string,
)
from ..const import (
    CONF_ANALOG_ATTRS,
    CONF_ANALOG_INPUT_ATTRIB_LIST,
    CONF_ZIGBEE_ID,
    ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
    AnalogAttrs,
    zigbee_ns,
)

AUTO_LOAD = ["zigbee"]

ZigbeeSensor = zigbee_ns.class_("ZigbeeSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    (
        sensor.sensor_schema(ZigbeeSensor)
        .extend(
            {
                cv.GenerateID(CONF_ANALOG_ATTRS): cv.declare_id(AnalogAttrs),
                cv.GenerateID(CONF_ANALOG_INPUT_ATTRIB_LIST): cv.declare_id(
                    cg.global_ns.namespace(
                        "ESPHOME_ZB_ZCL_DECLARE_ANALOG_INPUT_ATTRIB_LIST"
                    )
                ),
                cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(cv.COMPONENT_SCHEMA)
        .extend(ZigbeeBaseSchema)
    ),
    consume_ep_slots,
)


@coroutine_with_priority(50.0)
async def to_code(config):
    analog_attrs = zigbee_new_variable(config[CONF_ANALOG_ATTRS])
    attr_list = zigbee_new_attr_list(
        config[CONF_ANALOG_INPUT_ATTRIB_LIST],
        zigbee_assign(analog_attrs.out_of_service, 0),
        zigbee_assign(analog_attrs.present_value, 0),
        zigbee_assign(analog_attrs.status_flags, 0),
        zigbee_set_string(analog_attrs.description, config[CONF_NAME]),
    )

    cluster_id, clusters = zigbee_new_cluster_list(
        config, [ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, attr_list)]
    )

    zigbee_register_ep(config, cluster_id, 2, clusters)

    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(bool)
        )
        cg.add(var.set_template(template_))

    cg.add(var.set_ep(config[KEY_EP_NUMBER]))
    cg.add(var.set_cluster_attributes(analog_attrs))
    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))


@automation.register_action(
    "sensor.zigbee.publish",
    sensor.SensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
            cv.Required(CONF_STATE): cv.templatable(cv.float_),
        }
    ),
)
async def sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, float)
    cg.add(var.set_state(template_))
    return var
