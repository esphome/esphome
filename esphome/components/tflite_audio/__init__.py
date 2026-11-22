"""Streaming audio kind for tflite_micro_runtime.

The model is loaded by the streaming consumer, which owns the feature extractor,
the sliding window and the MicroResourceVariables. This kind only carries the tuning
values and the decoder for the windowed probabilities.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_micro_runtime import (
    PER_MODEL_SCHEMA,
    declare_model,
    final_validate,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_audio"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

CONF_PROBABILITY_CUTOFF = "probability_cutoff"
CONF_SLIDING_WINDOW_SIZE = "sliding_window_size"
CONF_FEATURES_STEP_SIZE = "features_step_size"
CONF_FEATURE_COUNT = "feature_count"
CONF_OUTPUT_PROCESSING = "output_processing"
CONF_SCALE_FACTOR = "scale_factor"
CONF_DECODER_ID = "decoder_id"

output_ns = cg.esphome_ns.namespace("tflite_output_processor")
ClassificationDecoder = output_ns.class_("ClassificationDecoder")
OutputProcessing = output_ns.enum("OutputProcessing", is_class=True)
OUTPUT_PROCESSING_MODES = {
    "argmax": OutputProcessing.OUTPUT_PROCESSING_ARGMAX,
    "softmax": OutputProcessing.OUTPUT_PROCESSING_SOFTMAX,
    "qat_quantized": OutputProcessing.OUTPUT_PROCESSING_QAT_QUANTIZED,
    "auto_detect": OutputProcessing.OUTPUT_PROCESSING_AUTO_DETECT,
}

AUDIO_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PROBABILITY_CUTOFF): cv.percentage,
        cv.Optional(CONF_SLIDING_WINDOW_SIZE): cv.positive_int,
        cv.Optional(CONF_FEATURES_STEP_SIZE): cv.int_range(min=0, max=30),
        cv.Optional(CONF_FEATURE_COUNT): cv.int_range(min=10, max=80),
        cv.Optional(CONF_OUTPUT_PROCESSING, default="argmax"): cv.one_of(
            *OUTPUT_PROCESSING_MODES, lower=True
        ),
        cv.Optional(CONF_SCALE_FACTOR, default=1.0): cv.float_range(min=0.1, max=100.0),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(ClassificationDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(AUDIO_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Pass the tuning values through and leave the load to the consumer."""
    cg.add(var.set_defer_load(True))
    if (cutoff := config.get(CONF_PROBABILITY_CUTOFF)) is not None:
        cg.add(var.set_probability_cutoff(cutoff))
    if (window := config.get(CONF_SLIDING_WINDOW_SIZE)) is not None:
        cg.add(var.set_sliding_window_size(window))
    if (step := config.get(CONF_FEATURES_STEP_SIZE)) is not None:
        cg.add(var.set_features_step_size(step))
    if (count := config.get(CONF_FEATURE_COUNT)) is not None:
        cg.add(var.set_feature_count(count))
    decoder = cg.new_Pvariable(
        config[CONF_DECODER_ID],
        OUTPUT_PROCESSING_MODES[config[CONF_OUTPUT_PROCESSING]],
        config[CONF_SCALE_FACTOR],
        config.get(CONF_DEBUG, False),
    )
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the tuning values and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
