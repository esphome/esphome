"""Model source resolution and metadata parsing for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; resolves .tflite model files from
local paths, git shorthands (github://, gitlab://, codeberg://) and
http(s):// URLs, and parses the companion .txt metadata reports /
filename heuristics shared by consumers.
"""

from .model_metadata import (
    datasize_to_bytes,
    infer_model_config_from_filename,
    parse_audio_txt_file,
    parse_model_txt_file,
    read_report,
)
from .model_source import MODEL_SOURCE_SCHEMA, resolve_model_source

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_model_source"

# Re-exported helpers for components that consume TFLite models: validation
# and resolution of model sources plus the metadata parsers.
__all__ = [
    "MODEL_SOURCE_SCHEMA",
    "datasize_to_bytes",
    "infer_model_config_from_filename",
    "parse_audio_txt_file",
    "parse_model_txt_file",
    "read_report",
    "resolve_model_source",
]
