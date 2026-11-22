"""Tabular pipeline for tflite_micro_runtime.

The model's input is a float32 feature vector; the output is a single regression
value ([1, 1]) or a class distribution.
"""

from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_tabular"

tflite_tabular_ns = cg.esphome_ns.namespace("tflite_tabular")
TabularInputPipeline = tflite_tabular_ns.class_("TabularInputPipeline")
TabularDecoder = tflite_tabular_ns.class_("TabularDecoder")

TABULAR_SCHEMA = cv.Schema({})

# Library component: the runtime creates the pipeline from its own entry.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(var, config: ConfigType, model_path: Path) -> None:
    """Create the feature pipeline and the output decoder for one model."""
    pipeline = cg.new_Pvariable(
        ID(
            f"{config[CONF_ID].id}_tabular",
            is_declaration=True,
            type=TabularInputPipeline,
        )
    )
    cg.add(var.set_input_pipeline(pipeline))
    decoder = cg.new_Pvariable(
        ID(
            f"{config[CONF_ID].id}_tabular_decoder",
            is_declaration=True,
            type=TabularDecoder,
        )
    )
    cg.add(var.set_output_decoder(decoder))
