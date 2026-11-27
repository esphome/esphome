"""Waveform pipeline for tflite_micro_runtime, for models such as noise reduction.

One frame of 16-bit PCM samples fills the model's input, and the model's single output
becomes a frame of 16-bit PCM samples. Framing and overlap stay in the consumer.
"""

import math
from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_micro_runtime import (
    CONF_PIPELINE_ID,
    PER_MODEL_SCHEMA,
    declare_model,
    final_validate,
)
from esphome.components.tflite_model_io.model_reader import TensorSpec, read_model_io
from esphome.components.tflite_model_source import resolve_model_source
from esphome.components.tflite_output_processor import CONF_DECODER_ID
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_waveform"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True

tflite_waveform_ns = cg.esphome_ns.namespace("tflite_waveform")
WaveformInputPipeline = tflite_waveform_ns.class_("WaveformInputPipeline")
WaveformDecoder = tflite_waveform_ns.class_("WaveformDecoder")

SAMPLE_TYPES = ("float32", "int8", "uint8")

WAVEFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PIPELINE_ID): cv.declare_id(WaveformInputPipeline),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(WaveformDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(WAVEFORM_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def _is_waveform(spec: TensorSpec) -> bool:
    """Whether a tensor is one batch of samples: [1, samples] or [1, samples, 1]."""
    shape = list(spec.shape)
    return (
        spec.type in SAMPLE_TYPES
        and len(shape) in (2, 3)
        and shape[0] == 1
        and shape[1] > 0
        and math.prod(shape[2:]) == 1
    )


def validate_model_io(model_path: Path) -> None:
    """Reject a model that does not turn one waveform frame into another."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    name = Path(str(model_path)).name
    for role, tensors in (("input", model_io.inputs), ("output", model_io.outputs)):
        if len(tensors) != 1:
            raise cv.Invalid(
                f"Model '{name}' declares {len(tensors)} {role} tensors; a waveform "
                f"model has one"
            )
        if not _is_waveform(tensors[0]):
            raise cv.Invalid(
                f"Model '{name}' declares the {role} {tensors[0].type} "
                f"{list(tensors[0].shape)}; a waveform model needs [1, samples] or "
                f"[1, samples, 1] of type {', '.join(SAMPLE_TYPES)}"
            )


def _final_validate(config: ConfigType) -> None:
    """Check the model's tensors and operators during config validation."""
    final_validate(config)
    model_path, _ = resolve_model_source(config)
    validate_model_io(model_path)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, _ = await declare_model(config)
    pipeline = cg.new_Pvariable(config[CONF_PIPELINE_ID])
    cg.add(var.set_input_pipeline(pipeline))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID])
    cg.add(var.set_output_decoder(decoder))
