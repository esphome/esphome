import logging

import hashlib
from pathlib import Path
import requests

import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.core import CORE, HexInt

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


def _validate_tflite_filename(value):
    value = cv.string(value)
    if not value.endswith(".tflite"):
        raise cv.Invalid("Filename must end with .tflite")
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
            cv.Required(CONF_FILE): _validate_tflite_filename,
            cv.Optional(CONF_REFRESH, default="1d"): cv.All(
                cv.string, cv.source_refresh
            ),
        }
    ),
    _process_git_source,
)


def _compute_local_file_path(config: dict) -> Path:
    url = config[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / key


def _process_http_source(config):
    url = config[CONF_URL]
    path = _compute_local_file_path(config)

    if not external_files.has_remote_file_changed(url, path):
        _LOGGER.debug("Remote file has not changed, skipping download")
        return config

    _LOGGER.debug(
        "Remote file has changed, downloading from %s to %s",
        url,
        path,
    )

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

    return config


HTTP_SCHEMA = cv.All(
    {
        cv.Required(CONF_URL): cv.url,
    },
    _process_http_source,
)

LOCAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.All(_validate_tflite_filename, cv.file_),
    }
)


def _validate_source_model_name(value):
    if not isinstance(value, str):
        raise cv.Invalid("Model name must be a string")

    if value.endswith(".tflite"):
        raise cv.Invalid("Model name must not end with .tflite")

    return MODEL_SOURCE_SCHEMA(
        {
            CONF_TYPE: TYPE_HTTP,
            CONF_URL: f"https://github.com/esphome/micro-wake-word-models/raw/main/models/{value}.tflite",
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
            cv.Optional(CONF_PROBABILITY_CUTOFF, default=0.5): cv.float_,
            cv.Optional(CONF_SLIDING_WINDOW_AVERAGE_SIZE, default=10): cv.positive_int,
            cv.Optional(CONF_ON_WAKE_WORD_DETECTED): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_MODEL): MODEL_SOURCE_SCHEMA,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_esp_idf,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic = await cg.get_variable(config[CONF_MICROPHONE])
    cg.add(var.set_microphone(mic))

    cg.add(var.set_probability_cutoff(config[CONF_PROBABILITY_CUTOFF]))
    cg.add(
        var.set_sliding_window_average_size(config[CONF_SLIDING_WINDOW_AVERAGE_SIZE])
    )

    if on_wake_word_detection_config := config.get(CONF_ON_WAKE_WORD_DETECTED):
        await automation.build_automation(
            var.get_wake_word_detected_trigger(),
            [(cg.std_string, "wake_word")],
            on_wake_word_detection_config,
        )

    esp32.add_idf_component(
        name="esp-tflite-micro",
        repo="https://github.com/espressif/esp-tflite-micro",
    )

    cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
    cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
    cg.add_build_flag("-DESP_NN")

    if model_config := config.get(CONF_MODEL):
        data = []
        if model_config[CONF_TYPE] == TYPE_GIT:
            # compute path to model file
            key = f"{model_config[CONF_URL]}@{model_config.get(CONF_REF)}"
            base_dir = Path(CORE.data_dir) / DOMAIN
            h = hashlib.new("sha256")
            h.update(key.encode())
            file: Path = base_dir / h.hexdigest()[:8] / model_config[CONF_FILE]
            # convert file to raw bytes array
            data = file.read_bytes()

        elif model_config[CONF_TYPE] == TYPE_LOCAL:
            with open(model_config[CONF_PATH], "rb") as f:
                data = f.read()

        elif model_config[CONF_TYPE] == TYPE_HTTP:
            path = _compute_local_file_path(model_config)
            with open(path, "rb") as f:
                data = f.read()

        rhs = [HexInt(x) for x in data]
        prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
        cg.add(var.set_model_start(prog_arr))


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
