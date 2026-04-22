import esphome.codegen as cg

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigbeeComponent = zigbee_ns.class_("ZigbeeComponent", cg.Component)
ZigbeeAttribute = zigbee_ns.class_("ZigbeeAttribute", cg.Component)
BinaryAttrs = zigbee_ns.struct("BinaryAttrs")
AnalogAttrs = zigbee_ns.struct("AnalogAttrs")
AnalogAttrsOutput = zigbee_ns.struct("AnalogAttrsOutput")

report = zigbee_ns.enum("ZigbeeReportT")
REPORT = {
    "coordinator": report.ZIGBEE_REPORT_COORDINATOR,
    "enable": report.ZIGBEE_REPORT_ENABLE,
    "force": report.ZIGBEE_REPORT_FORCE,
}

CONF_ON_JOIN = "on_join"
CONF_WIPE_ON_BOOT = "wipe_on_boot"
CONF_REPORT = "report"
CONF_ROUTER = "router"
CONF_POWER_SOURCE = "power_source"
POWER_SOURCE = {
    "UNKNOWN": "ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN",
    "MAINS_SINGLE_PHASE": "ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE",
    "MAINS_THREE_PHASE": "ZB_ZCL_BASIC_POWER_SOURCE_MAINS_THREE_PHASE",
    "BATTERY": "ZB_ZCL_BASIC_POWER_SOURCE_BATTERY",
    "DC_SOURCE": "ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE",
    "EMERGENCY_MAINS_CONST": "ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_CONST",
    "EMERGENCY_MAINS_TRANSF": "ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_TRANSF",
}

KEY_ZIGBEE = "zigbee"
