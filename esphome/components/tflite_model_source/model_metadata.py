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

# The audio pipeline handles the int8 features that the wake-word front end
# produces, so int8 models are usable there as well as uint8 and float32.
_SUPPORTED_AUDIO_INPUT_TYPES = {"float32", "uint8", "int8"}

# The value after the key is validated on its own so that a size written
# without a unit, or with a trailing note, is reported instead of ignored. It is
# anchored to the end of the line: text the parser cannot read must raise rather
# than be dropped, which would configure a different arena size.
_ARENA_SIZE_RE = re.compile(
    r"Recommended tensor_arena_size:\s*([^\r\n]*)", re.IGNORECASE
)
_ARENA_VALUE_RE = re.compile(r"^\s*([\d.]+\s*(?:[KMG]B?|bytes?|B)?)\s*$", re.IGNORECASE)
# [batch, height, width, channels] for colour models, [batch, height, width]
# for grayscale ones. The dtype may follow on the next line.
_IMAGE_INPUT_RE = re.compile(
    r"Input\s+0:\s+\[\s*\d+\s+(\d+)\s+(\d+)(?:\s+(\d+))?\][^\r\n]*"
    r"(?:\r?\n[ \t]*)?numpy\.(\w+)"
)
# Used when the shape cannot be read, so the dtype checks still run.
_INPUT_DTYPE_RE = re.compile(r"Input\s+0:[^\r\n]*numpy\.(\w+)")
_IMAGE_OUTPUT_RE = re.compile(r"Output\s+0:\s+\[\s*\d+\s+(\d+)\]")
# Any output line, so an output shape the parser cannot read is reported rather
# than silently leaving the class scale factor at its default.
_OUTPUT_LINE_RE = re.compile(r"Output\s+0:")
# [batch, time, features] for the audio models of the wake-word front end. A
# channel dimension may follow, so [1 49 40 1] is read the same way.
_AUDIO_INPUT_RE = re.compile(
    r"Input\s+0:\s+\[\s*\d+\s+(\d+)\s+(\d+)(?:\s+\d+)?\][^\r\n]*"
    r"(?:\r?\n[ \t]*)?numpy\.(\w+)"
)
_OPERATIONS_RE = re.compile(r"Total operations:\s+(\d+)")
# A zero count says the report found no delegate, so it must not reject the
# model.
_DELEGATE_RE = re.compile(r"Found [1-9]\d* DELEGATE operation")
_SOFTMAX_RE = re.compile(r"^\s+SOFTMAX:\s+\d+", re.MULTILINE)
_INT8_WEIGHT_RE = re.compile(r"<class 'numpy\.(int8|uint8)'>")
_PROBABILITY_CUTOFF_RE = re.compile(r"probability_cutoff:\s*([\d.]+)")
_FEATURES_STEP_SIZE_RE = re.compile(r"features_step_size:\s*(\d+)")


def datasize_to_bytes(value):
    try:
        text = re.sub(r"\bbytes?\b", "B", str(value).strip(), flags=re.IGNORECASE)
        text = text.upper()
        for suffix, factor in (
            ("GB", 1024**3),
            ("MB", 1024**2),
            ("KB", 1024),
            # Single-letter forms such as "64K" are common in reports.
            ("G", 1024**3),
            ("M", 1024**2),
            ("K", 1024),
            ("B", 1),
        ):
            if text.endswith(suffix):
                return int(float(text[: -len(suffix)].strip()) * factor)
        return int(float(text))
    except ValueError as e:
        raise cv.Invalid(f"Invalid data size: {value}") from e


def _read_report(txt_path):
    """Read a model report, reporting an unreadable file as a config error."""
    try:
        return txt_path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as err:
        raise cv.Invalid(f"Could not read the model report {txt_path}: {err}") from err


def _parse_arena_size(content, txt_path):
    """Return the recommended arena size in bytes, in whatever unit it is given."""
    if (match := _ARENA_SIZE_RE.search(content)) is None:
        return None
    value = match.group(1).strip()
    if (size_match := _ARENA_VALUE_RE.match(value)) is None:
        raise cv.Invalid(
            f"Could not read the tensor_arena_size {value!r} from {txt_path.name}"
        )
    return datasize_to_bytes(size_match.group(1))


def _check_input_type(dtype, model_path, supported, kind=""):
    if dtype not in supported:
        raise cv.Invalid(
            f"Model '{Path(model_path).name}' has unsupported {kind}input type "
            f"'{dtype}'. Supported input types: {', '.join(sorted(supported))}"
        )


def _check_hybrid_quantization(dtype, content, model_path):
    if dtype == "float32" and _INT8_WEIGHT_RE.search(content):
        raise cv.Invalid(
            f"Model '{Path(model_path).name}' uses hybrid quantization "
            "(float32 I/O + int8 weights).\n"
            "  TFLite Micro on ESP32 does NOT support hybrid execution.\n"
            "  Use a full integer quantized model instead."
        )


def _check_delegate_ops(content, model_path):
    if _DELEGATE_RE.search(content):
        raise cv.Invalid(
            f"Model '{Path(model_path).name}' contains DELEGATE operations, which "
            "TFLite Micro cannot run.\n"
            "  Re-export the model with the XNNPACK delegate disabled."
        )


def _parse_number(pattern, content, txt_path, field, convert):
    """Convert a matched report value, naming the file and field on failure."""
    if (match := pattern.search(content)) is None:
        return None
    try:
        return convert(match.group(1))
    except ValueError as err:
        raise cv.Invalid(
            f"Model report {txt_path.name} has an invalid {field}: {match.group(1)!r}"
        ) from err


def _scale_factor_for_classes(num_classes):
    """Class index scale factor: 100-class models report tens rather than units."""
    return 10.0 if num_classes == 100 else 1.0


def _scale_factor_from_filename(model_path):
    """The scale factor a ``_100cls`` file name tag implies."""
    name = Path(model_path).stem
    num_classes = 100 if "_100cls_" in name or name.endswith("_100cls") else 10
    return _scale_factor_for_classes(num_classes)


def parse_model_txt_file(model_path):
    """Parse an image-model ``.txt`` report, or return None if it is unusable."""
    txt_path = Path(model_path).with_suffix(".txt")
    if not txt_path.exists():
        return None
    content = _read_report(txt_path)

    # The dtype and quantisation checks run even when the shape cannot be read,
    # so a report this parser cannot use still rejects a model that could never
    # run, instead of handing it to the filename heuristics.
    loose_dtype = None
    if (dtype_match := _INPUT_DTYPE_RE.search(content)) is not None:
        loose_dtype = dtype_match.group(1)
        _check_input_type(loose_dtype, model_path, _SUPPORTED_INPUT_TYPES)
        _check_hybrid_quantization(loose_dtype, content, model_path)
    _check_delegate_ops(content, model_path)

    input_match = _IMAGE_INPUT_RE.search(content)
    if input_match is None:
        _LOGGER.warning(
            "Could not parse the input tensor from %s, falling back to the filename "
            "heuristics",
            txt_path.name,
        )
        return None

    dtype = input_match.group(4)
    if dtype != loose_dtype:
        _check_input_type(dtype, model_path, _SUPPORTED_INPUT_TYPES)
        _check_hybrid_quantization(dtype, content, model_path)

    config = {
        "input_height": int(input_match.group(1)),
        "input_width": int(input_match.group(2)),
        # A 3-D input has no channel dimension and is grayscale.
        "input_channels": int(input_match.group(3) or 1),
        "input_type": dtype,
    }
    if (output_match := _IMAGE_OUTPUT_RE.search(content)) is not None:
        config["scale_factor"] = _scale_factor_for_classes(int(output_match.group(1)))
    elif _OUTPUT_LINE_RE.search(content) is not None:
        # An output shape this parser cannot read must not silently leave the
        # default, which is wrong for a 100-class model.
        config["scale_factor"] = _scale_factor_from_filename(model_path)
        _LOGGER.warning(
            "Model report %s has an output shape this parser cannot read; taking the "
            "class scale factor %s from the file name instead",
            txt_path.name,
            config["scale_factor"],
        )
    if (arena_size := _parse_arena_size(content, txt_path)) is not None:
        config["tensor_arena_size"] = arena_size
    if (ops_match := _OPERATIONS_RE.search(content)) is not None:
        config["max_operators"] = int(ops_match.group(1)) + 5
    # A model that ends in SOFTMAX already outputs probabilities, so the class
    # index is the argmax of the tensor; otherwise the softmax is applied here.
    config["output_processing"] = "argmax" if _SOFTMAX_RE.search(content) else "softmax"
    return config


def parse_audio_txt_file(model_path):
    """Parse an audio-model ``.txt`` report, or return None if it is unusable."""
    txt_path = Path(model_path).with_suffix(".txt")
    if not txt_path.exists():
        return None
    content = _read_report(txt_path)

    # The dtype and quantisation checks run even when the shape cannot be read,
    # so a report this parser cannot use still rejects a model that could never
    # run, instead of leaving it to the YAML options.
    loose_dtype = None
    if (dtype_match := _INPUT_DTYPE_RE.search(content)) is not None:
        loose_dtype = dtype_match.group(1)
        _check_input_type(
            loose_dtype, model_path, _SUPPORTED_AUDIO_INPUT_TYPES, "audio "
        )
        _check_hybrid_quantization(loose_dtype, content, model_path)
    _check_delegate_ops(content, model_path)

    input_match = _AUDIO_INPUT_RE.search(content)
    if input_match is None:
        _LOGGER.warning(
            "Could not parse the input tensor from %s, leaving the audio configuration "
            "to the YAML options",
            txt_path.name,
        )
        return None

    dtype = input_match.group(3)
    if dtype != loose_dtype:
        _check_input_type(dtype, model_path, _SUPPORTED_AUDIO_INPUT_TYPES, "audio ")
        _check_hybrid_quantization(dtype, content, model_path)

    config = {
        "sliding_window_size": int(input_match.group(1)),
        "feature_count": int(input_match.group(2)),
    }
    if (arena_size := _parse_arena_size(content, txt_path)) is not None:
        config["tensor_arena_size"] = arena_size
    if (
        cutoff := _parse_number(
            _PROBABILITY_CUTOFF_RE, content, txt_path, "probability_cutoff", float
        )
    ) is not None:
        config["probability_cutoff"] = cutoff
    if (
        step_size := _parse_number(
            _FEATURES_STEP_SIZE_RE, content, txt_path, "features_step_size", int
        )
    ) is not None:
        config["features_step_size"] = step_size
    return config


def infer_model_config_from_filename(model_filename):
    config = {}
    name = Path(model_filename).stem
    guessed = []
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
        guessed.append("3-channel RGB input")
    # A "_10cls" tag is the default, so only "_100cls" changes the scale.
    num_classes = 100 if "_100cls_" in name or name.endswith("_100cls") else 10
    if num_classes == 10 and "_10cls" not in name:
        guessed.append("10 classes")
    if guessed:
        # These defaults are wrong for grayscale and 100-class models, so the
        # guesses must be visible in the build log.
        _LOGGER.warning(
            "Model '%s' has no file name tag for %s; assuming the defaults. "
            "Rename the file or supply its .txt report to be explicit",
            Path(model_filename).name,
            " or ".join(guessed),
        )
    config["scale_factor"] = _scale_factor_for_classes(num_classes)
    return config
