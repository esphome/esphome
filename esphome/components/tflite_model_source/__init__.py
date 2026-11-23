"""Resolve TFLite model sources: local paths, git shorthands and URLs.

Library-only component: no user-facing configuration. Loaded as a dependency
of tflite_micro_runtime.
"""

from .model_metadata import datasize_to_bytes
from .model_source import MODEL_SOURCE_SCHEMA, resolve_model_source

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_model_source"

__all__ = [
    "MODEL_SOURCE_SCHEMA",
    "datasize_to_bytes",
    "resolve_model_source",
]
