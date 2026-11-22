"""Text pipeline for tflite_micro_runtime.

A text model takes token ids, an attention mask and a segment mask as int32 tensors,
and returns logits or start/end positions. The tokenizer stays in the consumer.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_micro_runtime import (
    PER_MODEL_SCHEMA,
    declare_model,
    final_validate,
)
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_text"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

CONF_MAX_SEQUENCE_LENGTH = "max_sequence_length"

tflite_text_ns = cg.esphome_ns.namespace("tflite_text")
TextInputPipeline = tflite_text_ns.class_("TextInputPipeline")
TextDecoder = tflite_text_ns.class_("TextDecoder")

CONF_PIPELINE_ID = "pipeline_id"
CONF_DECODER_ID = "decoder_id"

TEXT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MAX_SEQUENCE_LENGTH): cv.positive_int,
        cv.GenerateID(CONF_PIPELINE_ID): cv.declare_id(TextInputPipeline),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(TextDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(TEXT_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Create the token pipeline and the output decoder for one model."""
    pipeline = cg.new_Pvariable(config[CONF_PIPELINE_ID])
    cg.add(var.set_input_pipeline(pipeline))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID])
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
