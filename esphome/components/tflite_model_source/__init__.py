"""Model source resolution and metadata parsing for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; resolves .tflite model files from
local paths, github:// shorthands and http(s):// URLs, and parses the
companion .txt metadata reports / filename heuristics shared by consumers.
"""

from .model_metadata import (
    _parse_audio_config,
    datasize_to_bytes,
    infer_model_config_from_filename,
    parse_model_txt_file,
)
from .model_source import MODEL_SOURCE_SCHEMA, resolve_model_source

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_model_source"

# Re-exported for consumers (meter_reader_tflite, micro_wake_word) which
# use the helpers previously exposed by the monolithic component.
__all__ = [
    "MODEL_SOURCE_SCHEMA",
    "_parse_audio_config",
    "datasize_to_bytes",
    "infer_model_config_from_filename",
    "parse_model_txt_file",
    "resolve_model_source",
]
