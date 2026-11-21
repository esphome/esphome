"""Configuration schema for the tflite_micro_runtime component."""

import esphome.codegen as cg
from esphome.components.tflite_image import IMAGE_SCHEMA
from esphome.components.tflite_model_source.model_metadata import datasize_to_bytes
from esphome.components.tflite_model_source.model_source import MODEL_SOURCE_SCHEMA
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_MODEL, CONF_RAW_DATA_ID

CONF_TENSOR_ARENA_SIZE = "tensor_arena_size"
CONF_MODEL_TYPE = "model_type"

tflite_micro_runtime_ns = cg.esphome_ns.namespace("tflite_micro_runtime")
# A Component, so setup() loads the configured model even without a consumer.
TFLiteMicroHelper = tflite_micro_runtime_ns.class_("TFLiteMicroHelper", cg.Component)

TENSOR_ARENA_SIZE_SCHEMA = cv.All(
    datasize_to_bytes, cv.Range(min=8 * 1024, max=2000 * 1024)
)

PER_MODEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TFLiteMicroHelper),
        cv.Required(CONF_MODEL): MODEL_SOURCE_SCHEMA,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        cv.Optional(CONF_TENSOR_ARENA_SIZE): TENSOR_ARENA_SIZE_SCHEMA,
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
        cv.Optional(CONF_MODEL_TYPE, default="raw"): cv.one_of("raw", "image"),
    }
).extend(IMAGE_SCHEMA)

MULTI_CONF = True
CONFIG_SCHEMA = cv.All(PER_MODEL_SCHEMA.extend(cv.COMPONENT_SCHEMA), cv.only_on_esp32)

# Static, not computed from CORE at import time: the module is imported once per
# process, so a CORE-derived value goes stale in a long-running dashboard.
DEPENDENCIES = ["esp32"]
