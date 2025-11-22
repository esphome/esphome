from esphome import automation
import esphome.codegen as cg

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigbeeComponent = zigbee_ns.class_("ZigbeeComponent", cg.Component)
ZigbeeAttribute = zigbee_ns.class_("ZigbeeAttribute", cg.Component)

FactoryResetAction = zigbee_ns.class_(
    "FactoryResetAction", automation.Action, cg.Parented.template(ZigbeeComponent)
)

report = zigbee_ns.enum("ZigbeeReportT")
REPORT = {
    "no": report.ZIGBEE_REPORT_NO,
    "yes": report.ZIGBEE_REPORT_YES,
    "force": report.ZIGBEE_REPORT_FORCE,
}

DEVICE_TYPE = "device_type"
ROLE = "role"
CONF_NUM = "num"
CONF_CLUSTERS = "clusters"
CONF_ATTRIBUTES = "attributes"
CONF_ENDPOINT = "endpoint"
CONF_CLUSTER = "cluster"
CONF_REPORT = "report"
CONF_ACCESS = "access"
CONF_SCALE = "scale"
CONF_ATTRIBUTE_ID = "attribute_id"
CONF_ROUTER = "router"

KEY_ZIGBEE = "zigbee"
KEY_BS_EP = "binary_sensor_ep"
