import esphome.codegen as cg

zigbee_ns = cg.esphome_ns.namespace("zigbee")
Zigbee = zigbee_ns.class_("Zigbee", cg.Component)

zb_char_t_ptr = cg.global_ns.namespace("zb_char_t *")

CONF_ZIGBEE_ID = "zigbee_id"
CONF_SWITCH = "switch"
CONF_MAX_EP_NUMBER = 8

zb_zcl_basic_attrs_ext_t = cg.global_ns.namespace("zb_zcl_basic_attrs_ext_t")
zb_zcl_identify_attrs_t = cg.global_ns.namespace("zb_zcl_identify_attrs_t")
zb_zcl_groups_attrs_t = cg.global_ns.namespace("zb_zcl_groups_attrs_t")
zb_zcl_scenes_attrs_t = cg.global_ns.namespace("zb_zcl_scenes_attrs_t")

CONF_BASIC_ATTRS_EXT = "basic_attrs_ext"
CONF_IDENTIFY_ATTRS = "identify_attrs"
CONF_GROUPS_ATTRS = "groups_attrs"
CONF_SCENES_ATTRS = "scenes_attrs"
CONF_BINARY_ATTRS = "binary_attrs"

CONF_BASIC_ATTRIB_LIST_EXT = "basic_attrib_list_ext"
CONF_IDENTIFY_ATTRIB_LIST = "identify_attrib_list"
CONF_GROUPS_ATTRIB_LIST = "groups_attrib_list"
CONF_SCENES_ATTRIB_LIST = "scenes_attrib_list"


# it has to be class to make use_id work
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT = cg.global_ns.class_(
    "ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT"
)
ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST = cg.global_ns.class_(
    "ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST"
)
ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST = cg.global_ns.class_(
    "ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST"
)
ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST = cg.global_ns.class_(
    "ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST"
)

# input/output
BinaryAttrs = zigbee_ns.struct("BinaryAttrs")

# input
CONF_BINARY_INPUT_ATTRIB_LIST = "binary_input_attrib_list"
CONF_BINARY_INPUT_CLUSTER_LIST = "binary_input_cluster_list"
CONF_BINARY_INPUT_EP = "binary_input_ep"
esphome_zb_ha_declare_binary_input_ep = cg.global_ns.namespace(
    "ESPHOME_ZB_HA_DECLARE_BINARY_INPUT_EP"
)

# output
CONF_BINARY_OUTPUT_ATTRIB_LIST = "binary_output_attrib_list"
CONF_BINARY_OUTPUT_CLUSTER_LIST = "binary_output_cluster_list"

CONF_BINARY_OUTPUT_EP = "binary_output_ep"
esphome_zb_ha_declare_binary_output_ep = cg.global_ns.namespace(
    "ESPHOME_ZB_HA_DECLARE_BINARY_OUTPUT_EP"
)
