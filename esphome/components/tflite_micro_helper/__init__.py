"""Component to manage TFLite Micro models and inference runtime.

Supports image (single-shot) and audio (streaming) model types,
with local file, github shorthand, and http URL model sources.
"""

import hashlib
import json
from pathlib import Path
import re
from urllib.parse import urljoin
import zlib

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEBUG,
    CONF_FILE,
    CONF_ID,
    CONF_INVERT,
    CONF_MODEL,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_REF,
    CONF_REFRESH,
    CONF_TYPE,
    CONF_URL,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE, HexInt

CODEOWNERS = ["@nliaudat"]

DOMAIN = "tflite_micro_helper"

tflite_micro_helper_ns = cg.esphome_ns.namespace("tflite_micro_helper")
TFLiteMicroHelper = tflite_micro_helper_ns.class_("TFLiteMicroHelper")

CONF_MODEL_TYPE = "model_type"
CONF_TENSOR_ARENA_SIZE = "tensor_arena_size"

CONF_INPUT_TYPE = "input_type"
CONF_INPUT_CHANNELS = "input_channels"
CONF_INPUT_WIDTH = "input_width"
CONF_INPUT_HEIGHT = "input_height"
CONF_OUTPUT_PROCESSING = "output_processing"
CONF_SCALE_FACTOR = "scale_factor"
CONF_INPUT_ORDER = "input_order"
CONF_NORMALIZE = "normalize"

CONF_PROBABILITY_CUTOFF = "probability_cutoff"
CONF_SLIDING_WINDOW_SIZE = "sliding_window_size"
CONF_FEATURES_STEP_SIZE = "features_step_size"
CONF_FEATURE_COUNT = "feature_count"

TYPE_HTTP = "http"


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


def _compute_local_file_path(url):
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    from esphome import external_files

    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / key


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


def _validate_source_shorthand(value):
    if not isinstance(value, str):
        raise cv.Invalid("Model source must be a string or dict")
    if Path(value).exists():
        return MODEL_SOURCE_SCHEMA({CONF_TYPE: TYPE_LOCAL, CONF_PATH: value})
    if value.endswith(".tflite"):
        return MODEL_SOURCE_SCHEMA({CONF_TYPE: TYPE_LOCAL, CONF_PATH: value})
    if value.startswith("github://"):
        from esphome import git

        git_file = git.GitFile.from_shorthand(value)
        conf = {
            CONF_TYPE: TYPE_GIT,
            CONF_URL: git_file.git_url,
            CONF_FILE: git_file.filename,
        }
        if git_file.ref:
            conf[CONF_REF] = git_file.ref
        try:
            return MODEL_SOURCE_SCHEMA(conf)
        except cv.Invalid as e:
            raise cv.Invalid(f"Could not resolve github:// model: {value}") from e
    if value.startswith(("http://", "https://")):
        return MODEL_SOURCE_SCHEMA({CONF_TYPE: TYPE_HTTP, CONF_URL: value})
    raise cv.Invalid(
        f"Cannot resolve model source: '{value}'. "
        f"Use a local .tflite path, github:// URL, or http(s):// URL."
    )


CV_GIT_SCHEMA = cv.GIT_SCHEMA
if isinstance(CV_GIT_SCHEMA, dict):
    CV_GIT_SCHEMA = cv.Schema(CV_GIT_SCHEMA)


def _process_git_source(config):
    from esphome import git

    repo_dir, _ = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config.get(CONF_REFRESH, "1d"),
        domain=DOMAIN,
    )
    if not (repo_dir / config[CONF_FILE]).exists():
        raise cv.Invalid(f"File does not exist in repository: {config[CONF_FILE]}")
    return config


GIT_SCHEMA = cv.All(
    CV_GIT_SCHEMA.extend(
        {
            cv.Required(CONF_FILE): cv.string,
            cv.Optional(CONF_REFRESH, default="1d"): cv.All(
                cv.string, cv.source_refresh
            ),
        }
    ),
    _process_git_source,
)


def _process_http_source(config):
    url = config[CONF_URL]
    path = _compute_local_file_path(url)
    from esphome import external_files

    json_path = path / "manifest.json"
    json_contents = external_files.download_content(url, json_path)
    manifest_data = json.loads(json_contents)
    model_file = manifest_data.get("model", "")
    if model_file:
        model_url = urljoin(url, model_file)
        model_path = path / Path(model_file).name
        external_files.download_content(str(model_url), model_path)
    return config


HTTP_SCHEMA = cv.All({cv.Required(CONF_URL): cv.url}, _process_http_source)

LOCAL_SCHEMA = cv.Schema({cv.Required(CONF_PATH): cv.All(cv.file_, cv.string)})

MODEL_SOURCE_SCHEMA = cv.Any(
    _validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: GIT_SCHEMA,
            TYPE_LOCAL: LOCAL_SCHEMA,
            TYPE_HTTP: HTTP_SCHEMA,
        }
    ),
    msg="Not a valid model path, github:// shorthand, or http(s):// URL",
)

PER_MODEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TFLiteMicroHelper),
        cv.Required(CONF_MODEL_TYPE): cv.enum(
            {"image": "image", "audio": "audio"}, lower=True
        ),
        cv.Required(CONF_MODEL): MODEL_SOURCE_SCHEMA,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        cv.Optional(CONF_TENSOR_ARENA_SIZE): cv.All(
            datasize_to_bytes, cv.Range(min=8 * 1024, max=2000 * 1024)
        ),
        cv.Optional(CONF_INPUT_TYPE): cv.enum(
            {"uint8": "uint8", "float32": "float32"}, lower=True
        ),
        cv.Optional(CONF_INPUT_CHANNELS): cv.int_range(min=1, max=4),
        cv.Optional(CONF_INPUT_WIDTH): cv.int_range(min=8, max=512),
        cv.Optional(CONF_INPUT_HEIGHT): cv.int_range(min=8, max=512),
        cv.Optional(CONF_OUTPUT_PROCESSING): cv.enum(
            {
                "direct_class": "direct_class",
                "softmax": "softmax",
                "argmax": "argmax",
                "logits": "logits",
                "qat_quantized": "qat_quantized",
                "experimental_scale": "experimental_scale",
                "logits_jomjol": "logits_jomjol",
                "softmax_jomjol": "softmax_jomjol",
                "auto_detect": "auto_detect",
            },
            lower=True,
        ),
        cv.Optional(CONF_SCALE_FACTOR): cv.float_range(min=0.1, max=100.0),
        cv.Optional(CONF_INPUT_ORDER): cv.enum(
            {"RGB": "RGB", "BGR": "BGR", "GRAY": "GRAY"}, upper=True
        ),
        cv.Optional(CONF_NORMALIZE): cv.boolean,
        cv.Optional(CONF_INVERT): cv.boolean,
        cv.Optional(CONF_PROBABILITY_CUTOFF): cv.percentage,
        cv.Optional(CONF_SLIDING_WINDOW_SIZE): cv.positive_int,
        cv.Optional(CONF_FEATURES_STEP_SIZE): cv.int_range(min=0, max=30),
        cv.Optional(CONF_FEATURE_COUNT): cv.int_range(min=10, max=80),
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
    }
)

MULTI_CONF = True
CONFIG_SCHEMA = PER_MODEL_SCHEMA

DEPENDENCIES = ["esp32"] if CORE.target_platform == "esp32" else []


def resolve_model_source(entry_config):
    model_spec = entry_config[CONF_MODEL]
    if isinstance(model_spec, str):
        return _load_local_file(model_spec)
    source_type = model_spec.get(CONF_TYPE, "local")
    if source_type == "local":
        return _load_local_file(model_spec[CONF_PATH])
    if source_type == "git":
        return _load_git_file(model_spec)
    if source_type == "http":
        return _load_http_file(model_spec)
    raise cv.Invalid(f"Unknown model source type: {source_type}")


def _load_local_file(path):
    model_path = CORE.relative_config_path(path)
    if not Path(model_path).exists():
        raise cv.Invalid(f"Model file not found: {model_path}")
    with Path(model_path).open("rb") as f:
        model_data = f.read()
    return model_path, model_data


def _load_git_file(config):
    from esphome import git

    repo_dir, _ = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config.get(CONF_REFRESH, "1d"),
        domain=DOMAIN,
    )
    model_path = repo_dir / config[CONF_FILE]
    if not model_path.exists():
        raise cv.Invalid(f"Model file not found in repository: {model_path}")
    with model_path.open("rb") as f:
        model_data = f.read()
    return model_path, model_data


def _load_http_file(config):
    from esphome import external_files

    url = config[CONF_URL]
    path = _compute_local_file_path(url)
    manifest_path = path / "manifest.json"
    json_contents = external_files.download_content(url, manifest_path)
    manifest_data = json.loads(json_contents)
    model_file = manifest_data.get("model")
    if not model_file:
        raise cv.Invalid(f"Manifest at {url} does not specify a model file")
    model_url = urljoin(url, model_file)
    model_path = path / Path(model_file).name
    external_files.download_content(str(model_url), model_path)
    with Path(model_path).open("rb") as f:
        model_data = f.read()
    return model_path, model_data


async def to_code(config):
    if CORE.target_platform == "esp32":
        cg.add_define("USE_TFLITE_MICRO_HELPER")
        model_type = config[CONF_MODEL_TYPE]
        model_path, model_data = resolve_model_source(config)
        var = cg.new_Pvariable(config[CONF_ID])
        rhs = [HexInt(x) for x in model_data]
        prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
        cg.add(var.set_model(prog_arr, len(model_data)))
        # Per-instance CRC32 - MULTI_CONF safe, each TFLiteMicroHelper carries its own expected checksum
        crc32_val = zlib.crc32(model_data) & 0xFFFFFFFF
        cg.add(var.set_expected_crc32(crc32_val))
        cg.add(var.set_model_type(model_type))
        cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
        cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
        cg.add_build_flag("-DESP_NN")
        cg.add_build_flag("-DOPTIMIZED_KERNEL=esp_nn")
        if config.get(CONF_DEBUG, False):
            cg.add_define("DEBUG_TFLITE_MICRO_HELPER")
            cg.add(var.set_debug(True))
        if model_type == "image":
            await _configure_image_model(config, var, model_path, model_data)
        elif model_type == "audio":
            await _configure_audio_model(config, var, model_path, model_data)
        txt_path = str(Path(model_path).with_suffix(".txt"))
        if Path(txt_path).exists():
            with Path(txt_path).open(encoding="utf-8") as f:
                txt_content = f.read()
            ops_match = re.search(r"Total operations:\s+(\d+)", txt_content)
            if ops_match:
                model_ops = int(ops_match.group(1)) + 5
                cg.add_build_flag(f"-DMAX_OPERATORS={model_ops}")
        else:
            cg.add_build_flag("-DMAX_OPERATORS=30")
        esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
        esp32.add_idf_component(name="espressif/esp-nn", ref="1.2.3")
        if model_type == "audio":
            esp32.add_idf_component(
                name="esphome/esp-micro-speech-features", ref="1.2.3"
            )


async def _configure_image_model(entry, var, model_path, model_data):
    model_filename = Path(str(model_path).replace("\\", "/")).name
    auto_config = parse_model_txt_file(model_path)
    if auto_config:
        print(f"  Auto-detected image config from '{model_filename}.txt':")
        for k, v in auto_config.items():
            print(f"    {k}: {v}")
    else:
        auto_config = infer_model_config_from_filename(model_filename)
        print(
            f"  No .txt file found, using filename heuristics for '{model_filename}':"
        )
        for k, v in auto_config.items():
            print(f"    {k}: {v}")
    yaml_overrides = {}
    for yaml_key, auto_key in [
        (CONF_INPUT_TYPE, "input_type"),
        (CONF_INPUT_CHANNELS, "input_channels"),
        (CONF_INPUT_WIDTH, "input_width"),
        (CONF_INPUT_HEIGHT, "input_height"),
        (CONF_OUTPUT_PROCESSING, "output_processing"),
        (CONF_SCALE_FACTOR, "scale_factor"),
        (CONF_INPUT_ORDER, "input_order"),
        (CONF_NORMALIZE, "normalize"),
        (CONF_INVERT, "invert"),
    ]:
        if yaml_key in entry:
            yaml_overrides[auto_key] = entry[yaml_key]
    if yaml_overrides:
        auto_config.update(yaml_overrides)
    cg.add(var.set_input_type(auto_config.get("input_type", "uint8")))
    cg.add(var.set_input_channels(auto_config.get("input_channels", 3)))
    cg.add(var.set_input_width(auto_config.get("input_width", 32)))
    cg.add(var.set_input_height(auto_config.get("input_height", 20)))
    cg.add(
        var.set_output_processing(auto_config.get("output_processing", "direct_class"))
    )
    cg.add(var.set_scale_factor(auto_config.get("scale_factor", 1.0)))
    cg.add(var.set_input_order(auto_config.get("input_order", "RGB")))
    cg.add(var.set_normalize(auto_config.get("normalize", False)))
    cg.add(var.set_invert(auto_config.get("invert", False)))
    if CONF_TENSOR_ARENA_SIZE in entry:
        cg.add(var.set_tensor_arena_size(entry[CONF_TENSOR_ARENA_SIZE]))
    elif "tensor_arena_size" in auto_config:
        cg.add(var.set_tensor_arena_size(auto_config["tensor_arena_size"]))


async def _configure_audio_model(entry, var, model_path, model_data):
    cg.add_define("USE_TFLITE_STREAMING")
    auto_config = _parse_audio_config(model_path) or {}
    if CONF_TENSOR_ARENA_SIZE in entry:
        cg.add(var.set_tensor_arena_size(entry[CONF_TENSOR_ARENA_SIZE]))
    elif "tensor_arena_size" in auto_config:
        cg.add(var.set_tensor_arena_size(auto_config["tensor_arena_size"]))
    if CONF_PROBABILITY_CUTOFF in entry:
        cg.add(var.set_probability_cutoff(entry[CONF_PROBABILITY_CUTOFF]))
    elif "probability_cutoff" in auto_config:
        cg.add(var.set_probability_cutoff(auto_config["probability_cutoff"]))
    if CONF_SLIDING_WINDOW_SIZE in entry:
        cg.add(var.set_sliding_window_size(entry[CONF_SLIDING_WINDOW_SIZE]))
    elif "sliding_window_size" in auto_config:
        cg.add(var.set_sliding_window_size(auto_config["sliding_window_size"]))
    if CONF_FEATURES_STEP_SIZE in entry:
        cg.add(var.set_features_step_size(entry[CONF_FEATURES_STEP_SIZE]))
    elif "features_step_size" in auto_config:
        cg.add(var.set_features_step_size(auto_config["features_step_size"]))
    if CONF_FEATURE_COUNT in entry:
        cg.add(var.set_feature_count(entry[CONF_FEATURE_COUNT]))
    elif "feature_count" in auto_config:
        cg.add(var.set_feature_count(auto_config["feature_count"]))


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
