"""Tabular pipeline for tflite_micro_runtime.

The model's input is a float32 feature vector; the output is a single regression
value ([1, 1]) or a class distribution.
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
DOMAIN = "tflite_tabular"

# The runtime is loaded for the model; only this component appears in the YAML.
AUTO_LOAD = ["tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

tflite_tabular_ns = cg.esphome_ns.namespace("tflite_tabular")
TabularInputPipeline = tflite_tabular_ns.class_("TabularInputPipeline")
TabularDecoder = tflite_tabular_ns.class_("TabularDecoder")

CONF_PIPELINE_ID = "pipeline_id"
CONF_DECODER_ID = "decoder_id"

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


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Create the feature pipeline and the output decoder for one model."""
    pipeline = cg.new_Pvariable(config[CONF_PIPELINE_ID])
    cg.add(var.set_input_pipeline(pipeline))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID])
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
