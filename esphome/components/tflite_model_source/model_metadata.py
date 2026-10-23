"""Model metadata parsing (companion .txt reports and filename heuristics).

Extracts image/audio model configuration from the ``.tflite`` companion
``.txt`` report and falls back to filename conventions.
"""

import logging
from pathlib import Path
import re

import esphome.config_validation as cv

_LOGGER = logging.getLogger(__name__)

# Input dtypes the image/audio pipelines can fill. Anything else (int8, int16,
# ...) would be interpreted with the wrong zero point and produce wrong
# results, so it is rejected instead of being reported as uint8.
_SUPPORTED_INPUT_TYPES = {"float32", "uint8"}

_ARENA_SIZE_RE = re.compile(
    r"Recommended tensor_arena_size:\s*([\d.]+\s*[KM]?B)", re.IGNORECASE
)
_IMAGE_INPUT_RE = re.compile(
    r"Input\s+0:\s+\[\s*\d+\s+(\d+)\s+(\d+)\s+(\d+)\].*?numpy\.(\w+)"
)
_IMAGE_OUTPUT_RE = re.compile(r"Output\s+0:\s+\[\s*\d+\s+(\d+)\]")
_AUDIO_INPUT_RE = re.compile(r"Input\s+0:\s+\[\s*\d+\s+(\d+)\s+(\d+)\].*?numpy\.(\w+)")
_OPERATIONS_RE = re.compile(r"Total operations:\s+(\d+)")
_DELEGATE_RE = re.compile(r"Found \d+ DELEGATE operation")
_SOFTMAX_RE = re.compile(r"^\s+SOFTMAX:\s+\d+", re.MULTILINE)
_INT8_WEIGHT_RE = re.compile(r"<class 'numpy\.(int8|uint8)'>")
_PROBABILITY_CUTOFF_RE = re.compile(r"probability_cutoff:\s*([\d.]+)")
_FEATURES_STEP_SIZE_RE = re.compile(r"features_step_size:\s*(\d+)")


def datasize_to_bytes(value):
    try:
        value = str(value).upper().strip()
        if value.endswith("KB"):
            return int(float(value[:-2]) * 1024)
        if value.endswith("MB"):
            return int(float(value[:-2]) * 1024 * 1024)
        if value.endswith("B"):
            return int(value[:-1])
        return int(value)
    except ValueError as e:
        raise cv.Invalid(f"Invalid data size: {e}") from e


def _parse_arena_size(content):
    """Return the recommended arena size in bytes, in whatever unit it is given."""
    match = _ARENA_SIZE_RE.search(content)
    if match is None:
        return None
    return datasize_to_bytes(re.sub(r"\s+", "", match.group(1)))


def parse_model_txt_file(model_path):
    """Parse an image-model ``.txt`` report, or return None if it is unusable."""
    txt_path = Path(model_path).with_suffix(".txt")
    if not txt_path.exists():
        return None
    content = txt_path.read_text(encoding="utf-8")

    input_match = _IMAGE_INPUT_RE.search(content)
    if input_match is None:
        _LOGGER.warning(
            "Could not parse the input tensor from %s, falling back to the filename "
            "heuristics",
            txt_path.name,
        )
        return None

    dtype = input_match.group(4)
    if dtype not in _SUPPORTED_INPUT_TYPES:
        raise cv.Invalid(
            f"Model '{Path(model_path).name}' has unsupported input type '{dtype}'. "
            f"Supported input types: {', '.join(sorted(_SUPPORTED_INPUT_TYPES))}"
        )

    config = {
        "input_height": int(input_match.group(1)),
        "input_width": int(input_match.group(2)),
        "input_channels": int(input_match.group(3)),
        "input_type": dtype,
    }
    output_match = _IMAGE_OUTPUT_RE.search(content)
    if output_match:
        num_classes = int(output_match.group(1))
        if num_classes == 10:
            config["scale_factor"] = 1.0
        elif num_classes == 100:
            config["scale_factor"] = 10.0
        else:
            config["scale_factor"] = 1.0
    if (arena_size := _parse_arena_size(content)) is not None:
        config["tensor_arena_size"] = arena_size
    if ops_match := _OPERATIONS_RE.search(content):
        config["max_operators"] = int(ops_match.group(1)) + 5
    if _DELEGATE_RE.search(content):
        _LOGGER.info("Model '%s' contains DELEGATE operations", txt_path.name)
    has_softmax = bool(_SOFTMAX_RE.search(content))
    config["output_processing"] = "direct_class" if has_softmax else "softmax"
    has_int8_weights = bool(_INT8_WEIGHT_RE.search(content))
    if dtype == "float32" and has_int8_weights:
        raise cv.Invalid(
            f"Model '{Path(model_path).name}' uses hybrid quantization "
            "(float32 I/O + int8 weights).\n"
            "  TFLite Micro on ESP32 does NOT support hybrid execution.\n"
            "  Use a full integer quantized model instead."
        )
    return config


def _parse_audio_config(model_path):
    """Parse an audio-model ``.txt`` report, or return None if it is unusable."""
    txt_path = Path(model_path).with_suffix(".txt")
    if not txt_path.exists():
        return None
    content = txt_path.read_text(encoding="utf-8")

    input_match = _AUDIO_INPUT_RE.search(content)
    if input_match is None:
        _LOGGER.warning(
            "Could not parse the input tensor from %s, leaving the audio configuration "
            "to the YAML options",
            txt_path.name,
        )
        return None

    config = {
        "sliding_window_size": int(input_match.group(1)),
        "feature_count": int(input_match.group(2)),
    }
    if (arena_size := _parse_arena_size(content)) is not None:
        config["tensor_arena_size"] = arena_size
    if prob_match := _PROBABILITY_CUTOFF_RE.search(content):
        config["probability_cutoff"] = float(prob_match.group(1))
    if step_match := _FEATURES_STEP_SIZE_RE.search(content):
        config["features_step_size"] = int(step_match.group(1))
    return config


def infer_model_config_from_filename(model_filename):
    config = {}
    name = Path(model_filename).stem
    if "_GRAY" in name or "_GRAYSCALE" in name:
        config["input_channels"] = 1
        config["input_order"] = "GRAY"
    elif "_RGB" in name:
        config["input_channels"] = 3
        config["input_order"] = "RGB"
    elif "_BGR" in name:
        config["input_channels"] = 3
        config["input_order"] = "BGR"
    else:
        config["input_channels"] = 3
        config["input_order"] = "RGB"
    if "_10cls_" in name or name.endswith("_10cls"):
        config["scale_factor"] = 1.0
    elif "_100cls_" in name or name.endswith("_100cls"):
        config["scale_factor"] = 10.0
    else:
        config["scale_factor"] = 1.0
    return config
