import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@kahrendt"]
audio_ns = cg.esphome_ns.namespace("audio")

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
)
