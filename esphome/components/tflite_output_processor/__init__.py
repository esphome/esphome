"""Output processing for TFLite Micro model results.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; converts raw model output tensors
into (value, confidence) pairs using the configured processing mode
(softmax, argmax, logits, auto_detect, ...).
"""

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["tflite_model_loader"]
DOMAIN = "tflite_output_processor"
