"""Streaming audio kind for tflite_micro_runtime.

The model is loaded by the streaming consumer, which owns the feature extractor,
the sliding window and the MicroResourceVariables. This kind only carries the tuning
values and the decoder for the windowed probabilities.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_micro_runtime import (
    CONF_TENSOR_ARENA_SIZE,
    PER_MODEL_SCHEMA,
    TFLiteMicroHelper,
    declare_model,
    final_validate,
)
from esphome.components.tflite_model_io.model_reader import read_model_io
from esphome.components.tflite_model_source import resolve_model_source
from esphome.components.tflite_output_processor import (
    CONF_DECODER_ID,
    CONF_OUTPUT_PROCESSING,
    CONF_SCALE_FACTOR,
    OUTPUT_PROCESSING,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_audio"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True

# Streaming state and resource tensors are not part of the model, so the
# generic estimator cannot size an audio arena.
AUDIO_DEFAULT_TENSOR_ARENA_SIZE = 100 * 1024

CONF_PROBABILITY_CUTOFF = "probability_cutoff"
CONF_SLIDING_WINDOW_SIZE = "sliding_window_size"
CONF_FEATURES_STEP_SIZE = "features_step_size"
CONF_FEATURE_COUNT = "feature_count"

output_ns = cg.esphome_ns.namespace("tflite_output_processor")
ClassificationDecoder = output_ns.class_("ClassificationDecoder")

tflite_audio_ns = cg.esphome_ns.namespace("tflite_audio")
# The audio kind extends the helper, so the entry's id is a TFLiteAudio and the
# streaming consumer can read the tuning values from it.
TFLiteAudio = tflite_audio_ns.class_("TFLiteAudio", TFLiteMicroHelper)

AUDIO_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PROBABILITY_CUTOFF): cv.percentage,
        cv.Optional(CONF_SLIDING_WINDOW_SIZE): cv.positive_int,
        cv.Optional(CONF_FEATURES_STEP_SIZE): cv.int_range(min=0, max=30),
        cv.Optional(CONF_FEATURE_COUNT): cv.int_range(min=10, max=80),
        cv.Optional(CONF_OUTPUT_PROCESSING, default="argmax"): cv.enum(
            OUTPUT_PROCESSING, lower=True
        ),
        cv.Optional(CONF_SCALE_FACTOR, default=1.0): cv.float_range(min=0.1, max=100.0),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(ClassificationDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend({cv.GenerateID(): cv.declare_id(TFLiteAudio)})
    .extend(AUDIO_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def validate_model_io(model_path: Path, config: ConfigType) -> int:
    """Reject an input the streaming consumer cannot fill; return its feature count."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    name = Path(str(model_path)).name
    if len(model_io.inputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.inputs)} input tensors; a "
            f"streaming audio model needs one"
        )
    spec = model_io.inputs[0]
    shape = list(spec.shape)
    if (
        len(shape) not in (3, 4)
        or spec.type not in ("int8", "uint8", "float32")
        or shape[0] != 1
        or any(dimension <= 0 for dimension in shape[1:])
    ):
        raise cv.Invalid(
            f"Model '{name}' declares the input {spec.type} {shape}; a streaming "
            f"audio model needs one batch with positive [frames, features] "
            f"dimensions, as [1, frames, features]"
        )
    feature_count = shape[-1]
    if (count := config.get(CONF_FEATURE_COUNT)) is not None and count != feature_count:
        raise cv.Invalid(
            f"feature_count is {count}, but the model's input has "
            f"{feature_count} features"
        )
    return feature_count


def _final_validate(config: ConfigType) -> None:
    """Check the audio input and the model's operators during config validation."""
    final_validate(config)
    model_path, _ = resolve_model_source(config)
    validate_model_io(model_path, config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Pass the tuning values through and leave the load to the consumer."""
    model_feature_count = validate_model_io(model_path, config)
    cg.add(var.set_defer_load(True))
    if (cutoff := config.get(CONF_PROBABILITY_CUTOFF)) is not None:
        cg.add(var.set_probability_cutoff(cutoff))
    if (window := config.get(CONF_SLIDING_WINDOW_SIZE)) is not None:
        cg.add(var.set_sliding_window_size(window))
    if (step := config.get(CONF_FEATURES_STEP_SIZE)) is not None:
        cg.add(var.set_features_step_size(step))
    # The consumer needs the model's feature dimension, not a generic default.
    cg.add(var.set_feature_count(config.get(CONF_FEATURE_COUNT, model_feature_count)))
    decoder = cg.new_Pvariable(
        config[CONF_DECODER_ID],
        config[CONF_OUTPUT_PROCESSING],
        config[CONF_SCALE_FACTOR],
        config.get(CONF_DEBUG, False),
    )
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the tuning values and decoder."""
    # Resolve the arena before declare_model() runs the generic estimator, which
    # rejects the resource tensors a streaming model can contain.
    if CONF_TENSOR_ARENA_SIZE not in config:
        config[CONF_TENSOR_ARENA_SIZE] = AUDIO_DEFAULT_TENSOR_ARENA_SIZE
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
