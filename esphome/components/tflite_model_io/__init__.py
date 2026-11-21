"""Read a TFLite model's flatbuffer without TensorFlow.

Library-only component: it reads the operators, the input and output tensors and an
arena estimate from a ``.tflite`` file, and is loaded by the components that need
them.
"""

import esphome.config_validation as cv

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_model_io"

# Library-only: the empty schema lets another component declare it in DEPENDENCIES.
CONFIG_SCHEMA = cv.Schema({})
