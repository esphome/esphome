import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@martgras", "@mebner86"]

# Define the namespace so ESPHome recognizes the component prefix
sen6x_ns = cg.esphome_ns.namespace("sen6x")

# Minimal schema to allow the component to be loaded
CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
)
