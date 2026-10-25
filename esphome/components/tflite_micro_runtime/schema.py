"""Configuration schemas for the tflite_micro_runtime component."""

import esphome.codegen as cg
from esphome.components.tflite_model_source.model_metadata import datasize_to_bytes
from esphome.components.tflite_model_source.model_source import MODEL_SOURCE_SCHEMA
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_INVERT, CONF_MODEL, CONF_RAW_DATA_ID

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
# A Component, so setup() loads the configured model and dump_config() reports it
# at boot even when no consumer references the model. An audio model is the
# exception: setup() leaves loading it to the streaming consumer.
TFLiteMicroHelper = tflite_micro_runtime_ns.class_("TFLiteMicroHelper", cg.Component)

# Closed name sets the helper stores as C++ enums. The configuration keeps the
# names, because they also select the code path here; to_code maps each one with
# the tables below.
ModelType = tflite_micro_runtime_ns.enum("ModelType", is_class=True)
InputType = tflite_micro_runtime_ns.enum("InputType", is_class=True)
InputOrder = tflite_micro_runtime_ns.enum("InputOrder", is_class=True)

MODEL_TYPES = {
    "image": ModelType.MODEL_TYPE_IMAGE,
    "audio": ModelType.MODEL_TYPE_AUDIO,
}

INPUT_TYPES = {
    "uint8": InputType.INPUT_TYPE_UINT8,
    "int8": InputType.INPUT_TYPE_INT8,
    "float32": InputType.INPUT_TYPE_FLOAT32,
}

INPUT_ORDERS = {
    "RGB": InputOrder.INPUT_ORDER_RGB,
    "BGR": InputOrder.INPUT_ORDER_BGR,
    "GRAY": InputOrder.INPUT_ORDER_GRAY,
}

# tflite_output_processor implements these four modes. The names below them come
# from the upstream project this component was ported from and map onto the mode
# they actually behaved like, so an existing configuration still validates without
# keeping five aliases in the C++ enum.
OUTPUT_PROCESSING_MODES = {
    "argmax": "argmax",
    "softmax": "softmax",
    "qat_quantized": "qat_quantized",
    "auto_detect": "auto_detect",
    "direct_class": "argmax",
    "logits": "softmax",
    "logits_jomjol": "softmax",
    "softmax_jomjol": "softmax",
    "experimental_scale": "softmax",
}

# Spellings the YAML options and a model's .txt report share. ``int8`` is not one
# of them: the image pipeline of tflite_model_source rejects an int8 model (its
# buffer would be filled with the wrong zero point), so no report can name it and
# only the C++ enumerator is kept for a consumer that sets it directly.
INPUT_TYPE_NAMES = {"uint8": "uint8", "float32": "float32"}
INPUT_ORDER_NAMES = {"RGB": "RGB", "BGR": "BGR", "GRAY": "GRAY"}

TENSOR_ARENA_SIZE_SCHEMA = cv.All(
    datasize_to_bytes, cv.Range(min=8 * 1024, max=2000 * 1024)
)

PER_MODEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TFLiteMicroHelper),
        cv.Required(CONF_MODEL_TYPE): cv.enum(
            {"image": "image", "audio": "audio"}, lower=True
        ),
        cv.Required(CONF_MODEL): MODEL_SOURCE_SCHEMA,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        cv.Optional(CONF_TENSOR_ARENA_SIZE): TENSOR_ARENA_SIZE_SCHEMA,
        cv.Optional(CONF_INPUT_TYPE): cv.enum(INPUT_TYPE_NAMES, lower=True),
        cv.Optional(CONF_INPUT_CHANNELS): cv.int_range(min=1, max=4),
        cv.Optional(CONF_INPUT_WIDTH): cv.int_range(min=8, max=512),
        cv.Optional(CONF_INPUT_HEIGHT): cv.int_range(min=8, max=512),
        cv.Optional(CONF_OUTPUT_PROCESSING): cv.enum(
            OUTPUT_PROCESSING_MODES, lower=True
        ),
        cv.Optional(CONF_SCALE_FACTOR): cv.float_range(min=0.1, max=100.0),
        cv.Optional(CONF_INPUT_ORDER): cv.enum(INPUT_ORDER_NAMES, upper=True),
        cv.Optional(CONF_NORMALIZE): cv.boolean,
        cv.Optional(CONF_INVERT): cv.boolean,
        cv.Optional(CONF_PROBABILITY_CUTOFF): cv.percentage,
        cv.Optional(CONF_SLIDING_WINDOW_SIZE): cv.positive_int,
        cv.Optional(CONF_FEATURES_STEP_SIZE): cv.int_range(min=0, max=30),
        cv.Optional(CONF_FEATURE_COUNT): cv.int_range(min=10, max=80),
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
    }
)

# The values a model's .txt report may supply, validated with the same rules as the
# YAML options of the same name. The report is written by the model exporter, so an
# unknown name in it would otherwise reach code generation and fail there with a
# KeyError instead of a configuration error. Keys this schema does not know are kept
# as they are: the operator count of an image report is used elsewhere in the build.
IMAGE_REPORT_VALUES_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_INPUT_TYPE): cv.enum(INPUT_TYPE_NAMES, lower=True),
        cv.Optional(CONF_INPUT_CHANNELS): cv.int_range(min=1, max=4),
        cv.Optional(CONF_INPUT_WIDTH): cv.int_range(min=8, max=512),
        cv.Optional(CONF_INPUT_HEIGHT): cv.int_range(min=8, max=512),
        cv.Optional(CONF_OUTPUT_PROCESSING): cv.enum(
            OUTPUT_PROCESSING_MODES, lower=True
        ),
        cv.Optional(CONF_SCALE_FACTOR): cv.float_range(min=0.1, max=100.0),
        cv.Optional(CONF_INPUT_ORDER): cv.enum(INPUT_ORDER_NAMES, upper=True),
        cv.Optional(CONF_NORMALIZE): cv.boolean,
        cv.Optional(CONF_INVERT): cv.boolean,
        cv.Optional(CONF_TENSOR_ARENA_SIZE): TENSOR_ARENA_SIZE_SCHEMA,
    },
    extra=cv.ALLOW_EXTRA,
)

AUDIO_REPORT_VALUES_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PROBABILITY_CUTOFF): cv.percentage,
        cv.Optional(CONF_SLIDING_WINDOW_SIZE): cv.positive_int,
        cv.Optional(CONF_FEATURES_STEP_SIZE): cv.int_range(min=0, max=30),
        cv.Optional(CONF_FEATURE_COUNT): cv.int_range(min=10, max=80),
        cv.Optional(CONF_TENSOR_ARENA_SIZE): TENSOR_ARENA_SIZE_SCHEMA,
    },
    extra=cv.ALLOW_EXTRA,
)

# Keyed by the ``model_type`` values of the configuration schema.
REPORT_VALUES_SCHEMAS = {
    "image": IMAGE_REPORT_VALUES_SCHEMA,
    "audio": AUDIO_REPORT_VALUES_SCHEMA,
}


MULTI_CONF = True
CONFIG_SCHEMA = cv.All(PER_MODEL_SCHEMA.extend(cv.COMPONENT_SCHEMA), cv.only_on_esp32)

# Static, not computed from CORE at import time: the module is imported once per
# process, so a value derived from CORE there goes stale (and is order dependent)
# in a long-running dashboard. CONFIG_SCHEMA already rejects every non-ESP32
# target, and the siblings are auto-loaded, so only the platform is listed here.
DEPENDENCIES = ["esp32"]
