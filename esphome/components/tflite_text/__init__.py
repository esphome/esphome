"""Text pipeline for tflite_micro_runtime.

A text model takes token ids, an attention mask and a segment mask as int32 tensors,
and returns logits or start/end positions. The tokenizer stays in the consumer.
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
DOMAIN = "tflite_text"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

CONF_POSITION_OUTPUT = "position_output"

tflite_text_ns = cg.esphome_ns.namespace("tflite_text")
TextInputPipeline = tflite_text_ns.class_("TextInputPipeline")
TextDecoder = tflite_text_ns.class_("TextDecoder")


TEXT_SCHEMA = cv.Schema(
    {
        # A two-entry output is ambiguous: two class logits or start/end positions.
        cv.Optional(CONF_POSITION_OUTPUT, default=False): cv.boolean,
        cv.GenerateID(CONF_PIPELINE_ID): cv.declare_id(TextInputPipeline),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(TextDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(TEXT_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def validate_model_io(model_path: Path, position_output: bool = False) -> None:
    """Reject a model whose tensors the text pipeline cannot fill."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    name = Path(str(model_path)).name
    if not 1 <= len(model_io.inputs) <= 3:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.inputs)} input tensors; a text "
            f"model has 1 to 3"
        )
    for index, spec in enumerate(model_io.inputs):
        if (
            spec.type != "int32"
            or len(spec.shape) != 2
            or spec.shape[0] != 1
            or spec.shape[1] <= 0
        ):
            raise cv.Invalid(
                f"Model '{name}' input {index} is {spec.type} {list(spec.shape)}; "
                f"a text model needs [1, length] int32 tensors"
            )
    if len(model_io.outputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.outputs)} output tensors; a "
            f"text model needs one"
        )
    output = model_io.outputs[0]
    if len(output.shape) != 2 or output.shape[0] != 1 or output.shape[1] <= 0:
        raise cv.Invalid(
            f"Model '{name}' declares the output {list(output.shape)}; the "
            f"decoder needs [1, classes]"
        )
    if position_output and output.shape[1] != 2:
        raise cv.Invalid(
            f"position_output needs a [1, 2] output (start and end), but model "
            f"'{name}' declares {list(output.shape)}"
        )


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Create the token pipeline and the output decoder for one model."""
    validate_model_io(model_path, config[CONF_POSITION_OUTPUT])
    pipeline = cg.new_Pvariable(config[CONF_PIPELINE_ID])
    cg.add(var.set_input_pipeline(pipeline))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID], config[CONF_POSITION_OUTPUT])
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
