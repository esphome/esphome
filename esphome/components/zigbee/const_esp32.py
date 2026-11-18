import esphome.codegen as cg

ALLOWED_UNITS = "allowed_units"
DEVICE_TYPE = "device_type"
ROLE = "role"
CONF_CLUSTERS = "clusters"
CONF_ATTRIBUTES = "attributes"
CONF_ATTRIBUTE_ID = "attribute_id"
KEY_ZIGBEE_ATTRIBUTES = "zigbee_attributes"
CONNECT = "connect"
SCALE = "scale"
KEY_ZIGBEE_EP = "zigbee_ep"
KEY_ZIGBEE_EP_NO_NUM = "zigbee_ep_no_num"
KEY_ZIGBEE_FIRST_EP_CL = "zigbee_first_ep_cl"

DEVICE_ID = {
    "ON_OFF_OUTPUT": cg.RawExpression("EZB_ZHA_ON_OFF_OUTPUT_DEVICE_ID"),
    "RANGE_EXTENDER": cg.RawExpression("EZB_ZHA_RANGE_EXTENDER_DEVICE_ID"),
    "MAINS_POWER_OUTLET": cg.RawExpression("EZB_ZHA_MAINS_POWER_OUTLET_DEVICE_ID"),
    "SIMPLE_SENSOR": cg.RawExpression("EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID"),
    "LIGHT_SENSOR": cg.RawExpression("EZB_ZHA_LIGHT_SENSOR_DEVICE_ID"),
    "OCCUPANCY_SENSOR": cg.RawExpression("EZB_ZHA_OCCUPANCY_SENSOR_DEVICE_ID"),
    "TEMPERATURE_SENSOR": cg.RawExpression("EZB_ZHA_TEMPERATURE_SENSOR_DEVICE_ID"),
    "PRESSURE_SENSOR": cg.RawExpression("EZB_ZHA_PRESSURE_SENSOR_DEVICE_ID"),
    "FLOW_SENSOR": cg.RawExpression("EZB_ZHA_FLOW_SENSOR_DEVICE_ID"),
    "CUSTOM_ATTR": 0xFFF2,
}
cluster_id = cg.esphome_ns.enum("ezb_zcl_cluster_id_e")
CLUSTER_ID = cluster_id.options(
    "BASIC",
    "ON_OFF",
    "TIME",
    "BINARY_INPUT",
    "BINARY_OUTPUT",
    "ANALOG_INPUT",
    "ANALOG_OUTPUT",
    "ILLUMINANCE_MEASUREMENT",
    "TEMPERATURE_MEASUREMENT",
    "PRESSURE_MEASUREMENT",
    "FLOW_MEASUREMENT",
    "REL_HUMIDITY_MEASUREMENT",
    "OCCUPANCY_SENSING",
    "CARBON_DIOXIDE_MEASUREMENT",
    "PM2_5_MEASUREMENT",
    prefix="EZB_ZCL_CLUSTER_ID_",
)
CLUSTER_ROLE = {
    "SERVER": cg.RawExpression("EZB_ZCL_CLUSTER_SERVER"),
    "CLIENT": cg.RawExpression("EZB_ZCL_CLUSTER_CLIENT"),
}
attr_type = cg.esphome_ns.enum("ezb_zcl_attr_type_e")
ATTR_TYPE = attr_type.options(
    "BOOL",
    "MAP8",
    "UINT8",
    "UINT16",
    "INT8",
    "INT16",
    "ENUM8",
    "ENUM16",
    "STRING",
    "SINGLE",
    "DOUBLE",
    prefix="EZB_ZCL_ATTR_TYPE_",
)
