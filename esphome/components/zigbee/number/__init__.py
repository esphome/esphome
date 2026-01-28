import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_LAMBDA,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_NAME,
    CONF_STEP,
)
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
    CONF_ANALOG_OUTPUT_ATTRIB_LIST,
    CONF_ZIGBEE_ID,
    ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT,
    AnalogAttrs,
    zigbee_ns,
)

AUTO_LOAD = ["zigbee"]

ZigbeeNumber = zigbee_ns.class_("ZigbeeNumber", number.Number, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    (
        number.number_schema(ZigbeeNumber)
        .extend(
            {
                cv.GenerateID(CONF_ANALOG_ATTRS): cv.declare_id(AnalogAttrs),
                cv.GenerateID(CONF_ANALOG_OUTPUT_ATTRIB_LIST): cv.declare_id(
                    cg.global_ns.namespace(
                        "ESPHOME_ZB_ZCL_DECLARE_ANALOG_OUTPUT_ATTRIB_LIST"
                    )
                ),
                cv.Optional(CONF_LAMBDA): cv.returning_lambda,
                cv.Required(CONF_MAX_VALUE): cv.float_,
                cv.Required(CONF_MIN_VALUE): cv.float_,
                cv.Required(CONF_STEP): cv.positive_float,
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(ZigbeeBaseSchema)
    ),
    consume_ep_slots,
)


@coroutine_with_priority(50.0)
async def to_code(config):
    analog_attrs = zigbee_new_variable(config[CONF_ANALOG_ATTRS])
    attr_list = zigbee_new_attr_list(
        config[CONF_ANALOG_OUTPUT_ATTRIB_LIST],
        zigbee_assign(analog_attrs.out_of_service, 0),
        zigbee_assign(analog_attrs.present_value, 0),
        zigbee_assign(analog_attrs.status_flags, 0),
        zigbee_set_string(analog_attrs.description, config[CONF_NAME]),
        zigbee_assign(analog_attrs.max_present_value, config[CONF_MAX_VALUE]),
        zigbee_assign(analog_attrs.min_present_value, config[CONF_MIN_VALUE]),
        zigbee_assign(analog_attrs.resolution, config[CONF_STEP]),
    )

    cluster_id, clusters = zigbee_new_cluster_list(
        config, [ZigbeeClusterDesc(ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, attr_list)]
    )

    zigbee_register_ep(config, cluster_id, 2, clusters)

    var = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
    await cg.register_component(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_template(template_))

    cg.add(var.set_ep(config[KEY_EP_NUMBER]))
    cg.add(var.set_cluster_attributes(analog_attrs))
    hub = await cg.get_variable(config[CONF_ZIGBEE_ID])
    cg.add(var.set_parent(hub))
