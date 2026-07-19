import hashlib
import json
import logging
from pathlib import Path
import re
from urllib.parse import urljoin

from esphome import automation, external_files, git
from esphome.automation import register_action, register_condition
from esphome.bundle import add_bundle_file
import esphome.codegen as cg
from esphome.components import esp32, microphone, ota, psram
from esphome.components.http_request import validate_url
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MICROPHONE,
    CONF_MODEL,
    CONF_PASSWORD,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_REF,
    CONF_REFRESH,
    CONF_TASK_STACK_IN_PSRAM,
    CONF_TYPE,
    CONF_URL,
    CONF_USERNAME,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE, HexInt
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["ring_buffer"]
CODEOWNERS = ["@kahrendt", "@jesserockz"]
DEPENDENCIES = ["microphone"]

DOMAIN = "micro_wake_word"


CONF_FEATURE_STEP_SIZE = "feature_step_size"
CONF_MODELS = "models"
CONF_ON_WAKE_WORD_DETECTED = "on_wake_word_detected"
CONF_PROBABILITY_CUTOFF = "probability_cutoff"
CONF_SLIDING_WINDOW_AVERAGE_SIZE = "sliding_window_average_size"
CONF_SLIDING_WINDOW_SIZE = "sliding_window_size"
CONF_STOP_AFTER_DETECTION = "stop_after_detection"
CONF_TENSOR_ARENA_SIZE = "tensor_arena_size"
CONF_VAD = "vad"

TYPE_HTTP = "http"

micro_wake_word_ns = cg.esphome_ns.namespace("micro_wake_word")

MicroWakeWord = micro_wake_word_ns.class_("MicroWakeWord", cg.Component)

DisableModelAction = micro_wake_word_ns.class_("DisableModelAction", automation.Action)
EnableModelAction = micro_wake_word_ns.class_("EnableModelAction", automation.Action)
StartAction = micro_wake_word_ns.class_("StartAction", automation.Action)
StopAction = micro_wake_word_ns.class_("StopAction", automation.Action)

ModelIsEnabledCondition = micro_wake_word_ns.class_(
    "ModelIsEnabledCondition", automation.Condition
)
IsRunningCondition = micro_wake_word_ns.class_(
    "IsRunningCondition", automation.Condition
)

WakeWordModel = micro_wake_word_ns.class_("WakeWordModel")


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


KEY_AUTHOR = "author"
KEY_MICRO = "micro"
KEY_MINIMUM_ESPHOME_VERSION = "minimum_esphome_version"
KEY_TRAINED_LANGUAGES = "trained_languages"
KEY_VERSION = "version"
KEY_WAKE_WORD = "wake_word"
KEY_WEBSITE = "website"

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

MANIFEST_SCHEMA_V2 = cv.Schema(
    {
        cv.Required(CONF_TYPE): "micro",
        cv.Required(CONF_MODEL): cv.string,
        cv.Required(KEY_AUTHOR): cv.string,
        cv.Required(KEY_VERSION): cv.All(cv.int_, 2),
        cv.Required(KEY_WAKE_WORD): cv.string,
        cv.Required(KEY_TRAINED_LANGUAGES): cv.ensure_list(cv.string),
        cv.Optional(KEY_WEBSITE): cv.url,
        cv.Required(KEY_MICRO): cv.Schema(
            {
                cv.Required(CONF_FEATURE_STEP_SIZE): cv.int_range(min=0, max=30),
                cv.Required(CONF_TENSOR_ARENA_SIZE): cv.int_,
                cv.Required(CONF_PROBABILITY_CUTOFF): cv.float_,
                cv.Required(CONF_SLIDING_WINDOW_SIZE): cv.positive_int,
                cv.Required(KEY_MINIMUM_ESPHOME_VERSION): cv.All(
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


def _convert_manifest_v1_to_v2(v1_manifest):
    v2_manifest = v1_manifest.copy()

    v2_manifest[KEY_VERSION] = 2
    v2_manifest[KEY_MICRO][CONF_SLIDING_WINDOW_SIZE] = v1_manifest[KEY_MICRO][
        CONF_SLIDING_WINDOW_AVERAGE_SIZE
    ]
    del v2_manifest[KEY_MICRO][CONF_SLIDING_WINDOW_AVERAGE_SIZE]

    # Original Inception-based V1 manifest models require a minimum of 45672 bytes
    v2_manifest[KEY_MICRO][CONF_TENSOR_ARENA_SIZE] = 45672
    # Original Inception-based V1 manifest models use a 20 ms feature step size
    v2_manifest[KEY_MICRO][CONF_FEATURE_STEP_SIZE] = 20
    # Original Inception-based V1 manifest models were trained only on TTS English samples
    v2_manifest[KEY_TRAINED_LANGUAGES] = ["en"]

    return v2_manifest


def _validate_manifest_version(manifest_data):
    if manifest_version := manifest_data.get(KEY_VERSION):
        if manifest_version == 1:
            try:
                MANIFEST_SCHEMA_V1(manifest_data)
            except cv.Invalid as e:
                raise cv.Invalid(f"Invalid manifest file: {e}") from e
        elif manifest_version == 2:
            try:
                MANIFEST_SCHEMA_V2(manifest_data)
            except cv.Invalid as e:
                raise cv.Invalid(f"Invalid manifest file: {e}") from e
        else:
            raise cv.Invalid("Invalid manifest version")
    else:
        raise cv.Invalid("Invalid manifest file, missing 'version' key")


HTTP_SCHEMA = cv.Schema(
    {
        # validate_url only accepts http(s); the shorthand validator relies
        # on this branch rejecting git shorthands ("github://...") so they
        # fall through to the git branch.
        cv.Required(CONF_URL): validate_url,
    }
)


def _register_local_model_file(config: ConfigType) -> ConfigType:
    """Register the model file that the manifest points to, so bundles include it.

    The manifest names its model file relative to itself, so that path never appears
    in the YAML and bundle discovery cannot find it on its own.

    Problems with the manifest are logged and ignored here rather than raised. Loading
    the manifest later reports them with better messages, and raising would be
    swallowed by the shorthand validator, which then reports a confusing error about a
    missing file in a git repository. Logging keeps the skipped registration
    diagnosable if the manifest is only briefly unreadable, since the bundle would
    then be built without the model file.
    """
    manifest_path: Path = config[CONF_PATH]
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        model = manifest[CONF_MODEL]
    except (OSError, ValueError, KeyError, TypeError) as err:
        _LOGGER.debug("Not registering a model file from %s: %s", manifest_path, err)
        return config
    if not isinstance(model, str):
        _LOGGER.debug(
            "Not registering a model file from %s: 'model' is %s, expected a string",
            manifest_path,
            type(model).__name__,
        )
        return config
    add_bundle_file(manifest_path.parent / model)
    return config


LOCAL_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_PATH): cv.All(_validate_json_filename, cv.file_),
        }
    ),
    _register_local_model_file,
)


# Bare model names in the official model repository ("okay_nabu"). Must not
# overlap with local paths, http(s) urls, or git shorthands
# ("github://user/repo/file.json@ref"), which the shorthand validator tries
# next; anything containing "/", ":" or "@" is not a model name.
_MODEL_NAME_RE = re.compile(r"[A-Za-z0-9_.-]+")


def _validate_source_model_name(value):
    if not isinstance(value, str):
        raise cv.Invalid("Model name must be a string")

    if value.endswith(".json"):
        raise cv.Invalid("Model name must not end with .json")

    if not _MODEL_NAME_RE.fullmatch(value):
        raise cv.Invalid("Model name may only contain letters, numbers, . _ -")

    return MODEL_SOURCE_SCHEMA(
        {
            CONF_TYPE: TYPE_HTTP,
            CONF_URL: f"https://github.com/esphome/micro-wake-word-models/raw/main/models/v2/{value}.json",
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

MODEL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(WakeWordModel),
        cv.Optional(CONF_MODEL): MODEL_SOURCE_SCHEMA,
        cv.Optional(CONF_PROBABILITY_CUTOFF): cv.percentage,
        cv.Optional(CONF_SLIDING_WINDOW_SIZE): cv.positive_int,
        cv.Optional(CONF_INTERNAL, default=False): cv.boolean,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)

# Provides a default VAD model that could be overridden
VAD_MODEL_SCHEMA = MODEL_SCHEMA.extend(
    cv.Schema(
        {
            cv.Optional(
                CONF_MODEL,
                default="vad",
            ): MODEL_SOURCE_SCHEMA,
        }
    )
)


def _maybe_empty_vad_schema(value):
    # Idea borrowed from uart/__init__.py's ``maybe_empty_debug`` function. Accessed 2 July 2024.
    # Loads a default VAD model without any parameters overridden.
    if value is None:
        value = {}
    return VAD_MODEL_SCHEMA(value)


def _download_http_models(config: ConfigType) -> ConfigType:
    """Download every http-sourced manifest and model file in two concurrent
    batches (all manifests, then all model files).

    The model file's URL only becomes known once its manifest has been
    fetched and parsed, so the two stages cannot be merged into one batch.
    """
    model_parameters = [*config[CONF_MODELS]]
    if vad := config.get(CONF_VAD):
        model_parameters.append(vad)
    # Keyed by cache path so a URL referenced twice is fetched and parsed once
    http_models: dict[Path, str] = {
        _compute_local_file_path(model_config): model_config[CONF_URL]
        for parameters in model_parameters
        if (model_config := parameters.get(CONF_MODEL)) is not None
        and model_config.get(CONF_TYPE) == TYPE_HTTP
    }
    if not http_models:
        return config

    external_files.download_content_many(
        ((url, path / "manifest.json") for path, url in http_models.items()),
        description="wake word manifest(s)",
    )

    model_files: list[tuple[str, Path]] = []
    errors: list[cv.Invalid] = []
    for path, url in http_models.items():
        try:
            manifest_data = json.loads((path / "manifest.json").read_bytes())
        except (OSError, ValueError) as e:
            errors.append(cv.Invalid(f"Invalid manifest file at {url}: {e}"))
            continue
        if not isinstance(manifest_data, dict):
            errors.append(
                cv.Invalid(f"Manifest file at {url} must contain a JSON object")
            )
            continue
        model = manifest_data.get(CONF_MODEL)
        if not isinstance(model, str):
            errors.append(
                cv.Invalid(f"Manifest file at {url} is missing the 'model' key")
            )
            continue
        model_files.append((urljoin(url, model), path / model))
    if errors:
        raise cv.MultipleInvalid(errors)

    external_files.download_content_many(model_files, description="wake word model(s)")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MicroWakeWord),
            cv.Optional(
                CONF_MICROPHONE, default={}
            ): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=16,
                min_channels=1,
                max_channels=1,
            ),
            cv.Required(CONF_MODELS): cv.ensure_list(
                cv.maybe_simple_value(MODEL_SCHEMA, key=CONF_MODEL)
            ),
            cv.Optional(CONF_ON_WAKE_WORD_DETECTED): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_VAD): _maybe_empty_vad_schema,
            cv.Optional(CONF_STOP_AFTER_DETECTION, default=True): cv.boolean,
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): psram.validate_task_stack_in_psram,
            cv.Optional(CONF_MODEL): cv.invalid(
                f"The {CONF_MODEL} parameter has moved to be a list element under the {CONF_MODELS} parameter."
            ),
            cv.Optional(CONF_PROBABILITY_CUTOFF): cv.invalid(
                f"The {CONF_PROBABILITY_CUTOFF} parameter has moved to be a list element under the {CONF_MODELS} parameter."
            ),
            cv.Optional(CONF_SLIDING_WINDOW_AVERAGE_SIZE): cv.invalid(
                f"The {CONF_SLIDING_WINDOW_AVERAGE_SIZE} parameter has been renamed to {CONF_SLIDING_WINDOW_SIZE} and moved to be a list element under the {CONF_MODELS} parameter."
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _download_http_models,
)


def _load_model_data(manifest_path: Path):
    with manifest_path.open(encoding="utf-8") as f:
        manifest = json.load(f)

    _validate_manifest_version(manifest)

    model_path = manifest_path.parent / manifest[CONF_MODEL]

    with model_path.open("rb") as f:
        model = f.read()

    if manifest.get(KEY_VERSION) == 1:
        manifest = _convert_manifest_v1_to_v2(manifest)

    return manifest, model


def _model_config_to_manifest_data(model_config):
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
        raise ValueError(f"Unsupported config type: {model_config[CONF_TYPE]}")

    return _load_model_data(file)


def _feature_step_size_validate(config):
    features_step_size = None

    for model_parameters in config[CONF_MODELS]:
        model_config = model_parameters.get(CONF_MODEL)
        manifest, _ = _model_config_to_manifest_data(model_config)

        model_step_size = manifest[KEY_MICRO][CONF_FEATURE_STEP_SIZE]

        if features_step_size is None:
            features_step_size = model_step_size
        elif features_step_size != model_step_size:
            raise cv.Invalid("Cannot load models with different features step sizes")


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(
                CONF_MICROPHONE
            ): microphone.final_validate_microphone_source_schema(
                "micro_wake_word", sample_rate=16000
            ),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    _feature_step_size_validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE])
    cg.add(var.set_microphone_source(mic_source))

    cg.add_define("USE_MICRO_WAKE_WORD")
    ota.request_ota_state_listeners()

    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        psram.request_external_task_stack()

    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.3~1")
    # Pin esp-nn for stable future builds (esp-tflite-micro depends on esp-nn)
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.1.2")

    esp32.add_idf_component(name="esphome/esp-micro-speech-features", ref="1.2.3")

    cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
    cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
    cg.add_build_flag("-DESP_NN")

    if vad_model := config.get(CONF_VAD):
        cg.add_define("USE_MICRO_WAKE_WORD_VAD")

        # Use the general model loading code for the VAD codegen
        config[CONF_MODELS].append(vad_model)

    for i, model_parameters in enumerate(config[CONF_MODELS]):
        model_config = model_parameters.get(CONF_MODEL)
        data = []
        manifest, data = _model_config_to_manifest_data(model_config)

        rhs = [HexInt(x) for x in data]
        prog_arr = cg.progmem_array(model_parameters[CONF_RAW_DATA_ID], rhs)

        probability_cutoff = model_parameters.get(
            CONF_PROBABILITY_CUTOFF, manifest[KEY_MICRO][CONF_PROBABILITY_CUTOFF]
        )
        quantized_probability_cutoff = int(probability_cutoff * 255)

        sliding_window_size = model_parameters.get(
            CONF_SLIDING_WINDOW_SIZE,
            manifest[KEY_MICRO][CONF_SLIDING_WINDOW_SIZE],
        )

        if manifest[KEY_WAKE_WORD] == "vad":
            cg.add(
                var.add_vad_model(
                    prog_arr,
                    quantized_probability_cutoff,
                    sliding_window_size,
                    manifest[KEY_MICRO][CONF_TENSOR_ARENA_SIZE],
                )
            )
        else:
            # Only enable the first wake word by default. After first boot, the enable state is saved/loaded to the flash
            default_enabled = i == 0
            wake_word_model = cg.new_Pvariable(
                model_parameters[CONF_ID],
                str(model_parameters[CONF_ID]),
                prog_arr,
                quantized_probability_cutoff,
                sliding_window_size,
                manifest[KEY_WAKE_WORD],
                manifest[KEY_MICRO][CONF_TENSOR_ARENA_SIZE],
                default_enabled,
                model_parameters[CONF_INTERNAL],
            )

            for lang in manifest[KEY_TRAINED_LANGUAGES]:
                cg.add(wake_word_model.add_trained_language(lang))

            cg.add(var.add_wake_word_model(wake_word_model))

    cg.add(var.set_features_step_size(manifest[KEY_MICRO][CONF_FEATURE_STEP_SIZE]))
    cg.add(var.set_stop_after_detection(config[CONF_STOP_AFTER_DETECTION]))

    if on_wake_word_detection_config := config.get(CONF_ON_WAKE_WORD_DETECTED):
        await automation.build_automation(
            var.get_wake_word_detected_trigger(),
            [(cg.std_string, "wake_word")],
            on_wake_word_detection_config,
        )


MICRO_WAKE_WORD_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(MicroWakeWord)})


@register_action(
    "micro_wake_word.start",
    StartAction,
    MICRO_WAKE_WORD_ACTION_SCHEMA,
    synchronous=True,
)
@register_action(
    "micro_wake_word.stop", StopAction, MICRO_WAKE_WORD_ACTION_SCHEMA, synchronous=True
)
@register_condition(
    "micro_wake_word.is_running", IsRunningCondition, MICRO_WAKE_WORD_ACTION_SCHEMA
)
async def micro_wake_word_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


MICRO_WAKE_WORLD_MODEL_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(WakeWordModel),
    }
)


@register_action(
    "micro_wake_word.enable_model",
    EnableModelAction,
    MICRO_WAKE_WORLD_MODEL_ACTION_SCHEMA,
    synchronous=True,
)
@register_action(
    "micro_wake_word.disable_model",
    DisableModelAction,
    MICRO_WAKE_WORLD_MODEL_ACTION_SCHEMA,
    synchronous=True,
)
@register_condition(
    "micro_wake_word.model_is_enabled",
    ModelIsEnabledCondition,
    MICRO_WAKE_WORLD_MODEL_ACTION_SCHEMA,
)
async def model_action(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
