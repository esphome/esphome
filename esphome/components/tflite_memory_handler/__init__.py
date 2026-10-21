"""Tensor arena memory management for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; provides PSRAM-aware tensor arena
allocation (MemoryManager) and arena size probing.
"""

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32", "tflite_op_processor"]
DOMAIN = "tflite_memory_handler"
