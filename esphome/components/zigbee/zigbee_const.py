import esphome.codegen as cg

ha_standard_devices = cg.esphome_ns.enum("esp_zb_ha_standard_devices_t")
DEVICE_ID = {
    "SIMPLE_SENSOR": ha_standard_devices.ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
    0x000C: ha_standard_devices.ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
    "TEST": ha_standard_devices.ESP_ZB_HA_TEST_DEVICE_ID,
    0xFFF0: ha_standard_devices.ESP_ZB_HA_TEST_DEVICE_ID,
    "CUSTOM_ATTR": ha_standard_devices.ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
    0xFFF2: ha_standard_devices.ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
}
cluster_id = cg.esphome_ns.enum("esp_zb_zcl_cluster_id_t")
CLUSTER_ID = {
    "BASIC": cluster_id.ESP_ZB_ZCL_CLUSTER_ID_BASIC,
    0x0000: cluster_id.ESP_ZB_ZCL_CLUSTER_ID_BASIC,
    "IDENTIFY": cluster_id.ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY,
    0x0005: cluster_id.ESP_ZB_ZCL_CLUSTER_ID_SCENES,
    "BINARY_INPUT": cluster_id.ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
    0x000F: cluster_id.ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT,
}
cluster_role = cg.esphome_ns.enum("esp_zb_zcl_cluster_role_t")
CLUSTER_ROLE = {
    "SERVER": cluster_role.ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    "CLIENT": cluster_role.ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE,
}
attr_type = cg.esphome_ns.enum("esp_zb_zcl_attr_type_t")
ATTR_TYPE = {
    "NULL": attr_type.ESP_ZB_ZCL_ATTR_TYPE_NULL,
    "BOOL": attr_type.ESP_ZB_ZCL_ATTR_TYPE_BOOL,
    "8BITMAP": attr_type.ESP_ZB_ZCL_ATTR_TYPE_8BITMAP,
    "U8": attr_type.ESP_ZB_ZCL_ATTR_TYPE_U8,
    "U16": attr_type.ESP_ZB_ZCL_ATTR_TYPE_U16,
    "U32": attr_type.ESP_ZB_ZCL_ATTR_TYPE_U32,
    "U64": attr_type.ESP_ZB_ZCL_ATTR_TYPE_U64,
    "S8": attr_type.ESP_ZB_ZCL_ATTR_TYPE_S8,
    "S16": attr_type.ESP_ZB_ZCL_ATTR_TYPE_S16,
    "S32": attr_type.ESP_ZB_ZCL_ATTR_TYPE_S32,
    "8BIT_ENUM": attr_type.ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM,
    "16BIT_ENUM": attr_type.ESP_ZB_ZCL_ATTR_TYPE_16BIT_ENUM,
    "SEMI": attr_type.ESP_ZB_ZCL_ATTR_TYPE_SEMI,
    "SINGLE": attr_type.ESP_ZB_ZCL_ATTR_TYPE_SINGLE,
    "DOUBLE": attr_type.ESP_ZB_ZCL_ATTR_TYPE_DOUBLE,
    "OCTET_STRING": attr_type.ESP_ZB_ZCL_ATTR_TYPE_OCTET_STRING,
    "CHAR_STRING": attr_type.ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING,
    "TIME_OF_DAY": attr_type.ESP_ZB_ZCL_ATTR_TYPE_TIME_OF_DAY,
    "DATE": attr_type.ESP_ZB_ZCL_ATTR_TYPE_DATE,
    "UTC_TIME": attr_type.ESP_ZB_ZCL_ATTR_TYPE_UTC_TIME,
    "CLUSTER_ID": attr_type.ESP_ZB_ZCL_ATTR_TYPE_CLUSTER_ID,
    "ATTRIBUTE_ID": attr_type.ESP_ZB_ZCL_ATTR_TYPE_ATTRIBUTE_ID,
    "BACNET_OID": attr_type.ESP_ZB_ZCL_ATTR_TYPE_BACNET_OID,
}
ATTR_ACCESS = {
    "READ_ONLY": 1,
    "WRITE_ONLY": 2,
    "READ_WRITE": 3,
}
