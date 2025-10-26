from esphome import automation
import esphome.codegen as cg

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigBeeComponent = zigbee_ns.class_("ZigBeeComponent", cg.Component)
ZigBeeAttribute = zigbee_ns.class_("ZigBeeAttribute", cg.Component)

ResetZigbeeAction = zigbee_ns.class_(
    "ResetZigbeeAction", automation.Action, cg.Parented.template(ZigBeeComponent)
)

report = zigbee_ns.enum("zigbee_report_t")
REPORT = {
    "no": report.ZIGBEE_REPORT_NO,
    "yes": report.ZIGBEE_REPORT_YES,
    "force": report.ZIGBEE_REPORT_FORCE,
}

CONF_DEVICE_TYPE = "device_type"
CONF_NUM = "num"
CONF_CLUSTERS = "clusters"
CONF_ATTRIBUTES = "attributes"
CONF_ROLE = "role"
CONF_ENDPOINT = "endpoint"
CONF_CLUSTER = "cluster"
CONF_REPORT = "report"
CONF_ACCESS = "access"
CONF_SCALE = "scale"
CONF_ATTRIBUTE_ID = "attribute_id"
CONF_ROUTER = "router"
