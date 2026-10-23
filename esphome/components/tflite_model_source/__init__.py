"""Model source resolution and metadata parsing for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; resolves .tflite model files from
local paths, github:// shorthands and http(s):// URLs, and parses the
companion .txt metadata reports / filename heuristics shared by consumers.
"""

import esphome.config_validation as cv

from .model_metadata import (
    _parse_audio_config,
    datasize_to_bytes,
    infer_model_config_from_filename,
    parse_model_txt_file,
)
from .model_source import MODEL_SOURCE_SCHEMA, resolve_model_source

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_model_source"

# Pure Python helper component: expose an empty config entry so the component
# can be loaded on its own (and by the component tests) without any YAML keys.
CONFIG_SCHEMA = cv.Schema({})

# Re-exported helpers for components that consume TFLite models: validation
# and resolution of model sources plus the metadata parsers.
__all__ = [
    "MODEL_SOURCE_SCHEMA",
    "_parse_audio_config",
    "datasize_to_bytes",
    "infer_model_config_from_filename",
    "parse_model_txt_file",
    "resolve_model_source",
]
