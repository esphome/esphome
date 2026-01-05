import esphome.codegen as cg

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigbeeComponent = zigbee_ns.class_("ZigbeeComponent", cg.Component)
ZigbeeAttribute = zigbee_ns.class_("ZigbeeAttribute", cg.Component)
BinaryAttrs = zigbee_ns.struct("BinaryAttrs")

report = zigbee_ns.enum("ZigbeeReportT")
REPORT = {
    "no": report.ZIGBEE_REPORT_NO,
    "yes": report.ZIGBEE_REPORT_YES,
    "force": report.ZIGBEE_REPORT_FORCE,
}

CONF_ON_JOIN = "on_join"
CONF_WIPE_ON_BOOT = "wipe_on_boot"
CONF_REPORT = "report"
CONF_ROUTER = "router"

KEY_ZIGBEE = "zigbee"
