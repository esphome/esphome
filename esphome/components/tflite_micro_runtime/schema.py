"""Configuration schemas for the tflite_micro_runtime component."""

import esphome.codegen as cg
from esphome.components.tflite_model_source.model_metadata import datasize_to_bytes
from esphome.components.tflite_model_source.model_source import MODEL_SOURCE_SCHEMA
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEBUG,
    CONF_INVERT,
    CONF_MODEL,
    CONF_RAW_DATA_ID,
    KEY_CORE,
)
from esphome.core import CORE

CONF_MODEL_TYPE = "model_type"
CONF_TENSOR_ARENA_SIZE = "tensor_arena_size"

CONF_INPUT_TYPE = "input_type"
CONF_INPUT_CHANNELS = "input_channels"
CONF_INPUT_WIDTH = "input_width"
CONF_INPUT_HEIGHT = "input_height"
CONF_OUTPUT_PROCESSING = "output_processing"
CONF_SCALE_FACTOR = "scale_factor"
CONF_INPUT_ORDER = "input_order"
CONF_NORMALIZE = "normalize"

CONF_PROBABILITY_CUTOFF = "probability_cutoff"
CONF_SLIDING_WINDOW_SIZE = "sliding_window_size"
CONF_FEATURES_STEP_SIZE = "features_step_size"
CONF_FEATURE_COUNT = "feature_count"

tflite_micro_runtime_ns = cg.esphome_ns.namespace("tflite_micro_runtime")
TFLiteMicroHelper = tflite_micro_runtime_ns.class_("TFLiteMicroHelper")

PER_MODEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TFLiteMicroHelper),
        cv.Required(CONF_MODEL_TYPE): cv.enum(
            {"image": "image", "audio": "audio"}, lower=True
        ),
        cv.Required(CONF_MODEL): MODEL_SOURCE_SCHEMA,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        cv.Optional(CONF_TENSOR_ARENA_SIZE): cv.All(
            datasize_to_bytes, cv.Range(min=8 * 1024, max=2000 * 1024)
        ),
        cv.Optional(CONF_INPUT_TYPE): cv.enum(
            {"uint8": "uint8", "float32": "float32"}, lower=True
        ),
        cv.Optional(CONF_INPUT_CHANNELS): cv.int_range(min=1, max=4),
        cv.Optional(CONF_INPUT_WIDTH): cv.int_range(min=8, max=512),
        cv.Optional(CONF_INPUT_HEIGHT): cv.int_range(min=8, max=512),
        cv.Optional(CONF_OUTPUT_PROCESSING): cv.enum(
            {
                "direct_class": "direct_class",
                "softmax": "softmax",
                "argmax": "argmax",
                "logits": "logits",
                "qat_quantized": "qat_quantized",
                "experimental_scale": "experimental_scale",
                "logits_jomjol": "logits_jomjol",
                "softmax_jomjol": "softmax_jomjol",
                "auto_detect": "auto_detect",
            },
            lower=True,
        ),
        cv.Optional(CONF_SCALE_FACTOR): cv.float_range(min=0.1, max=100.0),
        cv.Optional(CONF_INPUT_ORDER): cv.enum(
            {"RGB": "RGB", "BGR": "BGR", "GRAY": "GRAY"}, upper=True
        ),
        cv.Optional(CONF_NORMALIZE): cv.boolean,
        cv.Optional(CONF_INVERT): cv.boolean,
        cv.Optional(CONF_PROBABILITY_CUTOFF): cv.percentage,
        cv.Optional(CONF_SLIDING_WINDOW_SIZE): cv.positive_int,
        cv.Optional(CONF_FEATURES_STEP_SIZE): cv.int_range(min=0, max=30),
        cv.Optional(CONF_FEATURE_COUNT): cv.int_range(min=10, max=80),
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
    }
)


MULTI_CONF = True
CONFIG_SCHEMA = cv.All(PER_MODEL_SCHEMA, cv.only_on_esp32)

DEPENDENCIES = (
    [
        "esp32",
        "tflite_memory_handler",
        "tflite_model_debug",
        "tflite_model_loader",
        "tflite_op_processor",
        "tflite_output_processor",
    ]
    if CORE.data.get(KEY_CORE) is not None and CORE.target_platform == "esp32"
    else []
)
