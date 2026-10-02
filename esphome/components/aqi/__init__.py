import esphome.codegen as cg
from esphome.components.const import CONF_AQI  # noqa: F401

CODEOWNERS = ["@jasstrong", "@ximex", "@freekode"]

aqi_ns = cg.esphome_ns.namespace("aqi")
AQICalculatorType = aqi_ns.enum("AQICalculatorType")
CONF_CALCULATION_TYPE = "calculation_type"
CONF_EXTENDED_RANGE = "extended_range"

AQI_CALCULATION_TYPE = {
    "CAQI": AQICalculatorType.CAQI_TYPE,
    "AQI": AQICalculatorType.AQI_TYPE,
}
