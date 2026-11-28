"""Tabular pipeline for tflite_micro_runtime.

The model's input is a float32 feature vector; the output is a single regression
value ([1, 1]) or a class distribution.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_micro_runtime import (
    CONF_PIPELINE_ID,
    PER_MODEL_SCHEMA,
    declare_model,
    final_validate,
)
from esphome.components.tflite_model_io.model_reader import read_model_io
from esphome.components.tflite_output_processor import CONF_DECODER_ID
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_tabular"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

tflite_tabular_ns = cg.esphome_ns.namespace("tflite_tabular")
TabularInputPipeline = tflite_tabular_ns.class_("TabularInputPipeline")
TabularDecoder = tflite_tabular_ns.class_("TabularDecoder")


TABULAR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_PIPELINE_ID): cv.declare_id(TabularInputPipeline),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(TabularDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(TABULAR_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def validate_model_io(model_path: Path) -> None:
    """Reject a model whose tensors the tabular pipeline cannot fill."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    name = Path(str(model_path)).name
    if len(model_io.inputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.inputs)} input tensors; the "
            f"tabular pipeline fills one feature vector"
        )
    feature = model_io.inputs[0]
    if (
        feature.type != "float32"
        or len(feature.shape) != 2
        or feature.shape[0] != 1
        or feature.shape[1] <= 0
    ):
        raise cv.Invalid(
            f"Model '{name}' declares the input {feature.type} "
            f"{list(feature.shape)}; the tabular pipeline needs a "
            f"[1, features] float32 tensor"
        )
    if len(model_io.outputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.outputs)} output tensors; a "
            f"tabular model needs one"
        )
    output = model_io.outputs[0]
    if len(output.shape) != 2 or output.shape[0] != 1 or output.shape[1] <= 0:
        raise cv.Invalid(
            f"Model '{name}' declares the output {list(output.shape)}; the "
            f"decoder needs [1, classes]"
        )


async def configure(var: cg.MockObj, config: ConfigType, model_path: Path) -> None:
    """Create the feature pipeline and the output decoder for one model."""
    validate_model_io(model_path)
    pipeline = cg.new_Pvariable(config[CONF_PIPELINE_ID])
    cg.add(var.set_input_pipeline(pipeline))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID])
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
