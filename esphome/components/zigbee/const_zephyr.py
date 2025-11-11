import esphome.codegen as cg

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigbeeComponent = zigbee_ns.class_("ZigbeeComponent", cg.Component)

zb_char_t_ptr = cg.global_ns.namespace("zb_char_t *")

CONF_MAX_EP_NUMBER = 8
CONF_EP = "ep"
CONF_ZIGBEE_ID = "zigbee_id"
CONF_ON_JOIN = "on_join"
CONF_WIPE_ON_BOOT = "wipe_on_boot"
KEY_ZIGBEE = "zigbee"
KEY_EP_NUMBER = "ep_number"
CONF_ZIGBEE_BINARY_SENSOR = "zigbee_binary_sensor"

zb_zcl_basic_attrs_ext_t = cg.global_ns.namespace("zb_zcl_basic_attrs_ext_t")
zb_zcl_identify_attrs_t = cg.global_ns.namespace("zb_zcl_identify_attrs_t")
zb_zcl_groups_attrs_t = cg.global_ns.namespace("zb_zcl_groups_attrs_t")
zb_zcl_scenes_attrs_t = cg.global_ns.namespace("zb_zcl_scenes_attrs_t")

CONF_BASIC_ATTRS_EXT = "basic_attrs_ext"
CONF_IDENTIFY_ATTRS = "identify_attrs"
CONF_GROUPS_ATTRS = "groups_attrs"
CONF_SCENES_ATTRS = "scenes_attrs"
CONF_BINARY_ATTRS = "binary_attrs"
CONF_TIME_ATTRS = "time_attrs"
CONF_ANALOG_ATTRS = "analog_attrs"

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
AnalogAttrs = zigbee_ns.struct("AnalogAttrs")


# input
CONF_BINARY_INPUT_ATTRIB_LIST = "binary_input_attrib_list"
CONF_ANALOG_INPUT_ATTRIB_LIST = "analog_input_attrib_list"


# output
CONF_BINARY_OUTPUT_ATTRIB_LIST = "binary_output_attrib_list"
CONF_ANALOG_OUTPUT_ATTRIB_LIST = "analog_output_attrib_list"

# time
CONF_TIME_ATTRIB_LIST = "time_attrib_list"

# general
ESPHOME_ZB_HA_DECLARE_EP = cg.global_ns.namespace("ESPHOME_ZB_HA_DECLARE_EP")
CONF_CLUSTER_LIST = "cluster_list"


# clusters
ZB_ZCL_CLUSTER_ID_BASIC = "ZB_ZCL_CLUSTER_ID_BASIC"
ZB_ZCL_CLUSTER_ID_IDENTIFY = "ZB_ZCL_CLUSTER_ID_IDENTIFY"
ZB_ZCL_CLUSTER_ID_GROUPS = "ZB_ZCL_CLUSTER_ID_GROUPS"
ZB_ZCL_CLUSTER_ID_SCENES = "ZB_ZCL_CLUSTER_ID_SCENES"

ZB_ZCL_CLUSTER_ID_BINARY_INPUT = "ZB_ZCL_CLUSTER_ID_BINARY_INPUT"
ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT = "ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT"
ZB_ZCL_CLUSTER_ID_TIME = "ZB_ZCL_CLUSTER_ID_TIME"
ZB_ZCL_CLUSTER_ID_ANALOG_INPUT = "ZB_ZCL_CLUSTER_ID_ANALOG_INPUT"
ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT = "ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT"
