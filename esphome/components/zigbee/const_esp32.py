import esphome.codegen as cg

DEVICE_TYPE = "device_type"
ROLE = "role"
CONF_MAX_EP_NUMBER = 239
CONF_NUM = "num"
CONF_CLUSTERS = "clusters"
CONF_ATTRIBUTES = "attributes"
CONF_ENDPOINT = "endpoint"
CONF_CLUSTER = "cluster"
SCALE = "scale"
CONF_ATTRIBUTE_ID = "attribute_id"
KEY_BS_EP = "binary_sensor_ep"
KEY_SENSOR_EP = "sensor_ep"

DEVICE_ID = {
    "RANGE_EXTENDER": cg.RawExpression("EZB_ZHA_RANGE_EXTENDER_DEVICE_ID"),
    "SIMPLE_SENSOR": cg.RawExpression("EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID"),
    "CUSTOM_ATTR": 0xFFF2,
}
cluster_id = cg.esphome_ns.enum("ezb_zcl_cluster_id_e")
CLUSTER_ID = {
    "BASIC": cluster_id.EZB_ZCL_CLUSTER_ID_BASIC,
    "BINARY_INPUT": cluster_id.EZB_ZCL_CLUSTER_ID_BINARY_INPUT,
    "ANALOG_INPUT": cluster_id.EZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
}
CLUSTER_ROLE = {
    "SERVER": cg.RawExpression("EZB_ZCL_CLUSTER_SERVER"),
}
attr_type = cg.esphome_ns.enum("ezb_zcl_attr_type_e")
ATTR_TYPE = {
    "BOOL": attr_type.EZB_ZCL_ATTR_TYPE_BOOL,
    "MAP8": attr_type.EZB_ZCL_ATTR_TYPE_MAP8,
    "STRING": attr_type.EZB_ZCL_ATTR_TYPE_STRING,
    "SINGLE": attr_type.EZB_ZCL_ATTR_TYPE_SINGLE,
    "DOUBLE": attr_type.EZB_ZCL_ATTR_TYPE_DOUBLE,
}
