import logging

import json
import hashlib
from urllib.parse import urljoin
from pathlib import Path
import requests

import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.core import CORE, HexInt, EsphomeError

from esphome.components import esp32, microphone
from esphome import automation, git, external_files
from esphome.automation import register_action, register_condition


from esphome.const import (
    __version__,
    CONF_ID,
    CONF_MICROPHONE,
    CONF_MODEL,
    CONF_URL,
    CONF_FILE,
    CONF_PATH,
    CONF_REF,
    CONF_REFRESH,
    CONF_TYPE,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_RAW_DATA_ID,
    TYPE_GIT,
    TYPE_LOCAL,
)


_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@kahrendt", "@jesserockz"]
DEPENDENCIES = ["microphone"]
DOMAIN = "micro_wake_word"

CONF_PROBABILITY_CUTOFF = "probability_cutoff"
CONF_SLIDING_WINDOW_AVERAGE_SIZE = "sliding_window_average_size"
CONF_ON_WAKE_WORD_DETECTED = "on_wake_word_detected"

TYPE_HTTP = "http"

micro_wake_word_ns = cg.esphome_ns.namespace("micro_wake_word")

MicroWakeWord = micro_wake_word_ns.class_("MicroWakeWord", cg.Component)

StartAction = micro_wake_word_ns.class_("StartAction", automation.Action)
StopAction = micro_wake_word_ns.class_("StopAction", automation.Action)

IsRunningCondition = micro_wake_word_ns.class_(
    "IsRunningCondition", automation.Condition
)


def _validate_json_filename(value):
    value = cv.string(value)
    if not value.endswith(".json"):
        raise cv.Invalid("Manifest filename must end with .json")
    return value


def _process_git_source(config):
    repo_dir, _ = git.clone_or_update(
        url=config[CONF_URL],
        ref=config.get(CONF_REF),
        refresh=config[CONF_REFRESH],
        domain=DOMAIN,
        username=config.get(CONF_USERNAME),
        password=config.get(CONF_PASSWORD),
    )

    if not (repo_dir / config[CONF_FILE]).exists():
        raise cv.Invalid("File does not exist in repository")

    return config


CV_GIT_SCHEMA = cv.GIT_SCHEMA
if isinstance(CV_GIT_SCHEMA, dict):
    CV_GIT_SCHEMA = cv.Schema(CV_GIT_SCHEMA)

GIT_SCHEMA = cv.All(
    CV_GIT_SCHEMA.extend(
        {
            cv.Required(CONF_FILE): _validate_json_filename,
            cv.Optional(CONF_REFRESH, default="1d"): cv.All(
                cv.string, cv.source_refresh
            ),
        }
    ),
    _process_git_source,
)

KEY_WAKE_WORD = "wake_word"
KEY_AUTHOR = "author"
KEY_WEBSITE = "website"
KEY_VERSION = "version"
KEY_MICRO = "micro"
KEY_MINIMUM_ESPHOME_VERSION = "minimum_esphome_version"

MANIFEST_SCHEMA_V1 = cv.Schema(
    {
        cv.Required(CONF_TYPE): "micro",
        cv.Required(KEY_WAKE_WORD): cv.string,
        cv.Required(KEY_AUTHOR): cv.string,
        cv.Required(KEY_WEBSITE): cv.url,
        cv.Required(KEY_VERSION): cv.All(cv.int_, 1),
        cv.Required(CONF_MODEL): cv.string,
        cv.Required(KEY_MICRO): cv.Schema(
            {
                cv.Required(CONF_PROBABILITY_CUTOFF): cv.float_,
                cv.Required(CONF_SLIDING_WINDOW_AVERAGE_SIZE): cv.positive_int,
                cv.Optional(KEY_MINIMUM_ESPHOME_VERSION): cv.All(
                    cv.version_number, cv.validate_esphome_version
                ),
            }
        ),
    }
)


def _compute_local_file_path(config: dict) -> Path:
    url = config[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / key


def _download_file(url: str, path: Path) -> bytes:
    if not external_files.has_remote_file_changed(url, path):
        _LOGGER.debug("Remote file has not changed, skipping download")
        return path.read_bytes()

    try:
        req = requests.get(
            url,
            timeout=external_files.NETWORK_TIMEOUT,
            headers={"User-agent": f"ESPHome/{__version__} (https://esphome.io)"},
        )
        req.raise_for_status()
    except requests.exceptions.RequestException as e:
        raise cv.Invalid(f"Could not download file from {url}: {e}") from e

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(req.content)
    return req.content


def _process_http_source(config):
    url = config[CONF_URL]
    path = _compute_local_file_path(config)

    json_path = path / "manifest.json"

    json_contents = _download_file(url, json_path)

    manifest_data = json.loads(json_contents)
    if not isinstance(manifest_data, dict):
        raise cv.Invalid("Manifest file must contain a JSON object")

    try:
        MANIFEST_SCHEMA_V1(manifest_data)
    except cv.Invalid as e:
        raise cv.Invalid(f"Invalid manifest file: {e}") from e

    model = manifest_data[CONF_MODEL]
    model_url = urljoin(url, model)

    model_path = path / model

    _download_file(str(model_url), model_path)

    return config


HTTP_SCHEMA = cv.All(
    {
        cv.Required(CONF_URL): cv.url,
    },
    _process_http_source,
)

LOCAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.All(_validate_json_filename, cv.file_),
    }
)


def _validate_source_model_name(value):
    if not isinstance(value, str):
        raise cv.Invalid("Model name must be a string")

    if value.endswith(".json"):
        raise cv.Invalid("Model name must not end with .json")

    return MODEL_SOURCE_SCHEMA(
        {
            CONF_TYPE: TYPE_HTTP,
            CONF_URL: f"https://github.com/esphome/micro-wake-word-models/raw/main/models/{value}.json",
        }
    )


def _validate_source_shorthand(value):
    if not isinstance(value, str):
        raise cv.Invalid("Shorthand only for strings")

    try:  # Test for model name
        return _validate_source_model_name(value)
    except cv.Invalid:
        pass

    try:  # Test for local path
        return MODEL_SOURCE_SCHEMA({CONF_TYPE: TYPE_LOCAL, CONF_PATH: value})
    except cv.Invalid:
        pass

    try:  # Test for http url
        return MODEL_SOURCE_SCHEMA({CONF_TYPE: TYPE_HTTP, CONF_URL: value})
    except cv.Invalid:
        pass

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
        raise cv.Invalid(
            f"Could not find file '{git_file.filename}' in the repository. Please make sure it exists."
        ) from e


MODEL_SOURCE_SCHEMA = cv.Any(
    _validate_source_shorthand,
    cv.typed_schema(
        {
            TYPE_GIT: GIT_SCHEMA,
            TYPE_LOCAL: LOCAL_SCHEMA,
            TYPE_HTTP: HTTP_SCHEMA,
        }
    ),
    msg="Not a valid model name, local path, http(s) url, or github shorthand",
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MicroWakeWord),
            cv.GenerateID(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
            cv.Optional(CONF_PROBABILITY_CUTOFF): cv.percentage,
            cv.Optional(CONF_SLIDING_WINDOW_AVERAGE_SIZE): cv.positive_int,
            cv.Optional(CONF_ON_WAKE_WORD_DETECTED): automation.validate_automation(
                single=True
            ),
            cv.Required(CONF_MODEL): MODEL_SOURCE_SCHEMA,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_esp_idf,
)


def _load_model_data(manifest_path: Path):
    with open(manifest_path, encoding="utf-8") as f:
        manifest = json.load(f)

    try:
        MANIFEST_SCHEMA_V1(manifest)
    except cv.Invalid as e:
        raise EsphomeError(f"Invalid manifest file: {e}") from e

    model_path = manifest_path.parent / manifest[CONF_MODEL]

    with open(model_path, "rb") as f:
        model = f.read()

    return manifest, model


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic = await cg.get_variable(config[CONF_MICROPHONE])
    cg.add(var.set_microphone(mic))

    if on_wake_word_detection_config := config.get(CONF_ON_WAKE_WORD_DETECTED):
        await automation.build_automation(
            var.get_wake_word_detected_trigger(),
            [(cg.std_string, "wake_word")],
            on_wake_word_detection_config,
        )

    esp32.add_idf_component(
        name="esp-tflite-micro",
        repo="https://github.com/espressif/esp-tflite-micro",
        ref="v1.3.1",
    )

    cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
    cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
    cg.add_build_flag("-DESP_NN")

    model_config = config.get(CONF_MODEL)
    data = []
    if model_config[CONF_TYPE] == TYPE_GIT:
        # compute path to model file
        key = f"{model_config[CONF_URL]}@{model_config.get(CONF_REF)}"
        base_dir = Path(CORE.data_dir) / DOMAIN
        h = hashlib.new("sha256")
        h.update(key.encode())
        file: Path = base_dir / h.hexdigest()[:8] / model_config[CONF_FILE]

    elif model_config[CONF_TYPE] == TYPE_LOCAL:
        file = Path(model_config[CONF_PATH])

    elif model_config[CONF_TYPE] == TYPE_HTTP:
        file = _compute_local_file_path(model_config) / "manifest.json"

    else:
        raise ValueError("Unsupported config type: {model_config[CONF_TYPE]}")

    manifest, data = _load_model_data(file)

    rhs = [HexInt(x) for x in data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    cg.add(var.set_model_start(prog_arr))

    probability_cutoff = config.get(
        CONF_PROBABILITY_CUTOFF, manifest[KEY_MICRO][CONF_PROBABILITY_CUTOFF]
    )
    cg.add(var.set_probability_cutoff(probability_cutoff))
    sliding_window_average_size = config.get(
        CONF_SLIDING_WINDOW_AVERAGE_SIZE,
        manifest[KEY_MICRO][CONF_SLIDING_WINDOW_AVERAGE_SIZE],
    )
    cg.add(var.set_sliding_window_average_size(sliding_window_average_size))

    cg.add(var.set_wake_word(manifest[KEY_WAKE_WORD]))


MICRO_WAKE_WORD_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(MicroWakeWord)})


@register_action("micro_wake_word.start", StartAction, MICRO_WAKE_WORD_ACTION_SCHEMA)
@register_action("micro_wake_word.stop", StopAction, MICRO_WAKE_WORD_ACTION_SCHEMA)
@register_condition(
    "micro_wake_word.is_running", IsRunningCondition, MICRO_WAKE_WORD_ACTION_SCHEMA
)
async def micro_wake_word_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
