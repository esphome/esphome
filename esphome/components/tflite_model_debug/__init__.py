"""Debug utilities for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; provides the DEBUG_TFLITE_MICRO_HELPER
diagnostics (tensor inspection, quantization analysis, parameter sweeping)
defined out-of-line on ModelHandler.
"""

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_model_debug"
