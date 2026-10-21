"""Operator resolution for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; its C++ sources are compiled into
the firmware and used by the model handler to register the operators
required by a model.
"""

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "tflite_op_processor"
