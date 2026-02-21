import esphome.codegen as cg

CODEOWNERS = ["@jasstrong", "@ximex", "@freekode"]

aqi_ns = cg.esphome_ns.namespace("aqi")
AQICalculatorType = aqi_ns.enum("AQICalculatorType")

CONF_AQI = "aqi"
CONF_CALCULATION_TYPE = "calculation_type"

AQI_CALCULATION_TYPE = {
    "CAQI": AQICalculatorType.CAQI_TYPE,
    "AQI": AQICalculatorType.AQI_TYPE,
}
