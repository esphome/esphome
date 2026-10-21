"""Model metadata parsing (companion .txt reports and filename heuristics).

Extracts image/audio model configuration from the ``.tflite`` companion
``.txt`` report and falls back to filename conventions.
"""

from pathlib import Path
import re

import esphome.config_validation as cv


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


def parse_model_txt_file(model_path):
    txt_path = str(Path(model_path).with_suffix(".txt"))
    if not Path(txt_path).exists():
        return None
    with Path(txt_path).open(encoding="utf-8") as f:
        content = f.read()
    config = {}
    input_match = re.search(
        r"Input\s+0:\s+\[\s*\d+\s+(\d+)\s+(\d+)\s+(\d+)\].*?numpy\.(\w+)", content
    )
    if input_match:
        config["input_height"] = int(input_match.group(1))
        config["input_width"] = int(input_match.group(2))
        config["input_channels"] = int(input_match.group(3))
        dtype = input_match.group(4)
        config["input_type"] = "float32" if dtype == "float32" else "uint8"
    output_match = re.search(r"Output\s+0:\s+\[\s*\d+\s+(\d+)\]", content)
    if output_match:
        num_classes = int(output_match.group(1))
        if num_classes == 10:
            config["scale_factor"] = 1.0
        elif num_classes == 100:
            config["scale_factor"] = 10.0
        else:
            config["scale_factor"] = 1.0
    arena_match = re.search(r"Recommended tensor_arena_size:\s+(\d+)KB", content)
    if arena_match:
        config["tensor_arena_size"] = int(arena_match.group(1)) * 1024
    ops_match = re.search(r"Total operations:\s+(\d+)", content)
    if ops_match:
        config["max_operators"] = int(ops_match.group(1)) + 5
    if re.search(r"Found \d+ DELEGATE operation", content):
        print(f"  Note: Model '{Path(txt_path).name}' contains DELEGATE ops.")
    has_softmax = bool(re.search(r"^\s+SOFTMAX:\s+\d+", content, re.MULTILINE))
    config["output_processing"] = "direct_class" if has_softmax else "softmax"
    input_dtype = config.get("input_type", "")
    has_float32_io = input_dtype == "float32"
    has_int8_weights = bool(re.search(r"<class 'numpy\.(int8|uint8)'>", content))
    if has_float32_io and has_int8_weights:
        model_name = Path(txt_path).stem + ".tflite"
        raise cv.Invalid(
            f"Model '{model_name}' uses hybrid quantization (float32 I/O + int8 weights).\n"
            f"  TFLite Micro on ESP32 does NOT support hybrid execution.\n"
            f"  Use a full integer quantized model instead."
        )
    return config


def _parse_audio_config(model_path):
    txt_path = str(Path(model_path).with_suffix(".txt"))
    if not Path(txt_path).exists():
        return None
    with Path(txt_path).open(encoding="utf-8") as f:
        content = f.read()
    config = {}
    input_match = re.search(
        r"Input\s+0:\s+\[\s*\d+\s+(\d+)\s+(\d+)\].*?numpy\.(\w+)", content
    )
    if input_match:
        config["sliding_window_size"] = int(input_match.group(1))
        config["feature_count"] = int(input_match.group(2))
    arena_match = re.search(r"Recommended tensor_arena_size:\s+(\d+)KB", content)
    if arena_match:
        config["tensor_arena_size"] = int(arena_match.group(1)) * 1024
    prob_match = re.search(r"probability_cutoff:\s*([\d.]+)", content)
    if prob_match:
        config["probability_cutoff"] = float(prob_match.group(1))
    step_match = re.search(r"features_step_size:\s*(\d+)", content)
    if step_match:
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
