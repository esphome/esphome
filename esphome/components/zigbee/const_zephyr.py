import esphome.codegen as cg

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigbeeComponent = zigbee_ns.class_("ZigbeeComponent", cg.Component)
BinaryAttrs = zigbee_ns.struct("BinaryAttrs")

CONF_MAX_EP_NUMBER = 8
CONF_ZIGBEE_ID = "zigbee_id"
CONF_ON_JOIN = "on_join"
CONF_WIPE_ON_BOOT = "wipe_on_boot"
CONF_ZIGBEE_BINARY_SENSOR = "zigbee_binary_sensor"

# Keys for CORE.data storage
KEY_ZIGBEE = "zigbee"
KEY_EP_NUMBER = "ep_number"

# External ZBOSS SDK types (just strings for codegen)
ZB_ZCL_BASIC_ATTRS_EXT_T = "zb_zcl_basic_attrs_ext_t"
ZB_ZCL_IDENTIFY_ATTRS_T = "zb_zcl_identify_attrs_t"

# Cluster IDs
ZB_ZCL_CLUSTER_ID_BASIC = "ZB_ZCL_CLUSTER_ID_BASIC"
ZB_ZCL_CLUSTER_ID_IDENTIFY = "ZB_ZCL_CLUSTER_ID_IDENTIFY"
ZB_ZCL_CLUSTER_ID_BINARY_INPUT = "ZB_ZCL_CLUSTER_ID_BINARY_INPUT"
