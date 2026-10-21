"""Model loading for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; loads .tflite model data into a TFLite
Micro interpreter (operators + tensor arena) and provides CRC32
verification of model data.
"""

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = [
    "tflite_memory_handler",
    "tflite_op_processor",
    "tflite_output_processor",
]
DOMAIN = "tflite_model_loader"
