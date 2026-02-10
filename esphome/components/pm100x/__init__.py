import esphome.codegen as cg

CODEOWNERS = ["@tuct", "@habbie"]

pm100x_ns = cg.esphome_ns.namespace("pm100x")
PM100XComponent = pm100x_ns.class_("PM100XComponent", cg.PollingComponent)

# Enum for PM100X models
PM100XModel = pm100x_ns.enum("PM100XModel", True)
MODEL_OPTIONS = {
    "pm1003": PM100XModel.PM1003,
    "pm1006": PM100XModel.PM1006,
    "pm1006k": PM100XModel.PM1006K,
}
