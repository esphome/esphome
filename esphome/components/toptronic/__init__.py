from dataclasses import dataclass, field
import logging
import pathlib

import yaml

import esphome.codegen as cg
from esphome.components import button as button_platform, ota
from esphome.components.canbus import CONF_CANBUS_ID, CanbusComponent
from esphome.components.const import CONF_DEVICE_TYPE
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_NAME,
    CONF_OPTIONS,
    CONF_PLATFORM,
)
from esphome.core import CORE, ID
from esphome.cpp_types import Component
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# button: the generated "Refresh all" button is always built


def AUTO_LOAD(config: list[ConfigType] | ConfigType | None = None) -> list[str]:
    """Load sensor, the button for Refresh all, and every domain a configured hub's presets use."""
    domains = {"button", "sensor"}
    if isinstance(config, dict):
        config = [config]
    for conf in config or []:
        for path in (PRESETS_DIR / _preset_device_type(conf)).glob("*_en.yaml"):
            with path.open(encoding="utf-8") as f:
                domains.update((yaml.safe_load(f) or {}).keys())
    return sorted(domains)


CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["canbus"]
DOMAIN = "toptronic"
MULTI_CONF = True


CONF_TOPTRONIC_ID = "toptronic_id"
CONF_DEVICE_ADDR = "device_addr"
CONF_FUNCTION_GROUP = "function_group"
CONF_FUNCTION_NUMBER = "function_number"
CONF_DATAPOINT = "datapoint"
CONF_DECIMAL = "decimal"
CONF_VALUES = "values"
CONF_LANGUAGE = "language"
CONF_MAX_PENDING_MESSAGES = "max_pending_messages"
CONF_MAX_FRAMES_PER_MESSAGE = "max_frames_per_message"
CONF_MAX_PENDING_AGE = "max_pending_age"
CONF_WRITE_MIN_INTERVAL = "write_min_interval"
CONF_REJECT_WRITES_BEFORE_READ = "reject_writes_before_read"
CONF_BOOT_REFRESH_DELAY = "boot_refresh_delay"
CONF_REFRESH_INTERVAL = "refresh_interval"
CONF_MAX_REFRESH_RETRIES = "max_refresh_retries"
CONF_REFRESH_RETRY_INTERVAL = "refresh_retry_interval"

LANGS = ("de", "en", "fr", "it")
# The main loop runs every 16 ms, so a shorter GET spacing is not achievable
REFRESH_INTERVAL_MIN_MS = 16
# Platforms that read a datapoint: one entity per datapoint per hub, or the first would never be polled
READ_DOMAINS = ("sensor", "text_sensor")
# Platforms that write a datapoint: one entity per datapoint per hub, or the first write would never be sent
WRITE_DOMAINS = ("number", "select", "button")
# A write entity takes its state from the read entity on the same datapoint, so the kinds have to match
LINKED_DOMAINS = {"number": "sensor", "select": "text_sensor"}

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)

TT_TYPE = toptronic.enum("TypeName")
_TT_TYPES = ("U8", "U16", "U32", "S8", "S16", "S32", "S64")
TT_TYPE_OPTIONS = {name: getattr(TT_TYPE, name) for name in _TT_TYPES}

DEVICE_TYPES = {
    "WEZ": 0,  # heat generator
    "SOL": 64,  # solar module
    "PS": 128,  # buffer storage tank
    "FW": 192,  # district heating
    "HK": 256,  # heating circuit
    "MWA": 384,  # energy meter module
    "GLT": 448,  # building management system
    "HV": 512,  # HomeVent ventilation
    "BM": 1024,  # control module (display)
    "GW": 1152,  # Modbus/KNX gateway
}
# Other names accepted for a device type
DEVICE_TYPE_ALIASES = {"BD": "BM"}


def validate_device_type(value: object) -> str:
    value = cv.string(value).upper()
    return cv.enum(DEVICE_TYPES)(DEVICE_TYPE_ALIASES.get(value, value))


def _preset_device_type(conf: ConfigType) -> str:
    """Return the preset directory of a hub config, before or after validation."""
    value = str(conf.get(CONF_DEVICE_TYPE, "")).upper()
    return DEVICE_TYPE_ALIASES.get(value, value)


PRESETS_DIR = pathlib.Path(__file__).parent / "presets"
_PRESETS_KEY = "toptronic_used_ids"


@dataclass
class TopTronicData:
    """Per-run state shared between final validation and code generation."""

    # Validated preset entity configs per hub ID (built in FINAL_VALIDATE_SCHEMA
    # so their Component IDs are recorded early enough to count into
    # ESPHOME_COMPONENT_COUNT).
    generated_entities: dict[str, list[tuple[str, ConfigType]]] = field(
        default_factory=dict
    )
    # Validated build-wide "Refresh all" button config, and whether a hub emitted it
    refresh_button: ConfigType | None = None
    refresh_button_generated: bool = False


def _get_data() -> TopTronicData:
    """Return this component's per-run state from CORE.data."""
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = TopTronicData()
    return CORE.data[DOMAIN]


def _shared_used_ids():
    """Build-wide set of used IDs, shared across all hub instances."""
    if _PRESETS_KEY not in CORE.data:
        # Seed with every ID name in the config tree (post ID pass, reached via
        # the final-validation context) so generated preset IDs cannot collide
        # with IDs the user already declared.
        from esphome import final_validate as fv
        from esphome.config import iter_ids

        result = fv.full_config.get()
        used = {id.id for id, _ in iter_ids(result) if id.id is not None}
        CORE.data[_PRESETS_KEY] = used
    return CORE.data[_PRESETS_KEY]


def _validate_options_values_lengths(config: ConfigType) -> ConfigType:
    """Reject select/text_sensor configs whose 'options' and 'values' differ in length."""
    if len(config[CONF_OPTIONS]) != len(config[CONF_VALUES]):
        raise cv.Invalid(
            f"'options' length ({len(config[CONF_OPTIONS])}) must match "
            f"'values' length ({len(config[CONF_VALUES])})"
        )
    return config


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Required(CONF_FUNCTION_GROUP): cv.uint8_t,
        cv.Required(CONF_FUNCTION_NUMBER): cv.uint8_t,
        cv.Required(CONF_DATAPOINT): cv.uint16_t,
    }
)


def config_schema_polling(update_interval: str = "30s"):
    """Base schema plus a polling interval, for the read entities (sensor, text_sensor).

    Write entities use CONFIG_SCHEMA_BASE: they never poll.
    """
    return CONFIG_SCHEMA_BASE.extend(cv.polling_component_schema(update_interval))


# Presets are optional: a device type without a preset directory can still be
# used with hand-written entities.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TopTronicComponent),
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Required(CONF_DEVICE_TYPE): validate_device_type,
        # Device types are 64 apart, so a larger address would alias the next type
        cv.Required(CONF_DEVICE_ADDR): cv.int_range(min=0, max=63),
        cv.Optional(CONF_LANGUAGE, default="en"): cv.one_of(*LANGS, lower=True),
        # Each pending message is a fixed reassembly slot of max_frames_per_message * 7 bytes
        cv.Optional(CONF_MAX_PENDING_MESSAGES, default=2): cv.int_range(min=1, max=16),
        # The 5-bit frame count field allows at most 31 frames
        cv.Optional(CONF_MAX_FRAMES_PER_MESSAGE, default=8): cv.int_range(
            min=3, max=31
        ),
        cv.Optional(
            CONF_MAX_PENDING_AGE, default="5000ms"
        ): cv.positive_time_period_milliseconds,
        # Minimum time between two writes to the same datapoint; 0 disables the limit
        cv.Optional(
            CONF_WRITE_MIN_INTERVAL, default="2s"
        ): cv.positive_time_period_milliseconds,
        # Refuse writes to a datapoint that has not answered a read since boot
        cv.Optional(CONF_REJECT_WRITES_BEFORE_READ, default=True): cv.boolean,
        # One-shot refresh of every hub after boot; 0s disables it
        cv.Optional(
            CONF_BOOT_REFRESH_DELAY, default="30s"
        ): cv.positive_time_period_milliseconds,
        # Spacing between two refresh GETs; the loop itself cannot go faster than its 16 ms period
        cv.Optional(CONF_REFRESH_INTERVAL, default="50ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=REFRESH_INTERVAL_MIN_MS)),
        ),
        # How often an unanswered refresh GET is sent again, and after how long
        cv.Optional(CONF_MAX_REFRESH_RETRIES, default=1): cv.int_range(min=0, max=10),
        cv.Optional(
            CONF_REFRESH_RETRY_INTERVAL, default="200ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)

# Reassembly storage is sized for the largest hub
_request_slot = cg.slot_counter("TOPTRONIC_SLOTS")
_request_frame = cg.slot_counter("TOPTRONIC_MAX_FRAMES")
_request_hub = cg.slot_counter("TOPTRONIC_HUB_COUNT")


def _final_validate(config: ConfigType) -> None:
    """Generate and validate this hub's preset entities at validation time."""
    _register_preset_entities(config)
    _register_refresh_button()
    _reject_duplicate_datapoints(config)
    _reject_mismatched_links(config)


FINAL_VALIDATE_SCHEMA = _final_validate


def _load_entities(device_type: str, language: str):
    """Load the preset files for one device type and language.

    Presets are shipped per language. Each kind falls back to English on its own
    when the requested language has no file for it, so a partial translation never
    drops the rest of the corpus, and a warning names the file that fell back so a
    missing translation is visible instead of silently yielding no entities.
    """
    directory = PRESETS_DIR / device_type
    kinds = ("sensors", "inputs", "buttons")

    entities = []
    for kind in kinds:
        path = directory / f"{kind}_{language}.yaml"
        if not path.exists():
            path = directory / f"{kind}_en.yaml"
            if not path.exists():
                continue
            if language != "en":
                _LOGGER.warning(
                    "No toptronic %s presets for device type %s in language '%s', "
                    "falling back to 'en'",
                    kind,
                    device_type,
                    language,
                )
        with path.open(encoding="utf-8") as f:
            data = yaml.safe_load(f) or {}
        for platform_name, entries in data.items():
            entities.extend((platform_name, dict(entry)) for entry in entries or [])
    return entities


def _resolve_ids(obj, used=None):
    """Resolve auto-generated IDs inside a validated entity config.

    Entities synthesized from presets bypass config.py's global ID pass, so
    nested IDs (e.g. filter IDs) keep id=None. Resolve them in final validation
    (after the ID pass) to avoid duplicate empty-ID registrations during code
    generation, and record Component-derived declarations in
    CORE.component_ids exactly as config.py's ID pass would.
    """
    if used is None:
        used = set()
    if isinstance(obj, ID):
        if obj.id is None:
            obj.resolve(used)
        elif obj.is_declaration and obj.id in used:
            raise cv.Invalid(f"ID '{obj.id}' is already used by another entity")
        used.add(obj.id)
        if (
            obj.is_declaration
            and isinstance(obj.type, cg.MockObjClass)
            and obj.type.inherits_from(Component)
        ):
            CORE.component_ids.add(obj.id)
    elif isinstance(obj, dict):
        for value in obj.values():
            _resolve_ids(value, used)
    elif isinstance(obj, (list, tuple)):
        for value in obj:
            _resolve_ids(value, used)
    return used


def _register_preset_entities(config: ConfigType) -> None:
    """Validate the preset entities and register them at validation time.

    The preset entity configs bypass config.py's global ID pass (they do not
    live in the config tree), so they are generated and validated here in
    FINAL_VALIDATE_SCHEMA. Component-derived IDs are recorded in
    CORE.component_ids now, because ESPHOME_COMPONENT_COUNT is emitted from the
    core component's to_code() (CoroPriority.CORE), which runs BEFORE any
    component to_code(): IDs registered later never make it into the define and
    their App::components_ push_back (StaticVector) is silently dropped, so
    those sensors would never poll.

    The validated configs are stashed in CORE.data for to_code to generate.
    """
    from . import button, number, select, sensor, text_sensor

    platforms = {
        "sensor": sensor.CONFIG_SCHEMA,
        "text_sensor": text_sensor.CONFIG_SCHEMA,
        "number": number.CONFIG_SCHEMA,
        "select": select.CONFIG_SCHEMA,
        "button": button.CONFIG_SCHEMA,
    }
    used_ids = _shared_used_ids()
    generated = []

    # Two hubs would otherwise generate identically named preset entities, which
    # ESPHome rejects. The device type alone does not separate them: the corpora
    # overlap across types (FW and MWA share 112 names, FW and WEZ 18), so two
    # hubs of different types collide just as two of the same type do. Qualify
    # the names with the device type and address whenever more than one hub is
    # configured, so the common single-hub names stay unchanged.
    from esphome import final_validate as fv

    hubs = fv.full_config.get().get(DOMAIN) or []
    prefix = (
        f"{config[CONF_DEVICE_TYPE]} {config[CONF_DEVICE_ADDR]}: "
        if len(hubs) > 1
        else ""
    )

    for platform_name, entity_conf in _load_entities(
        config[CONF_DEVICE_TYPE], config[CONF_LANGUAGE]
    ):
        if platform_name not in platforms:
            raise cv.Invalid(
                f"Unsupported platform '{platform_name}' in toptronic preset"
            )
        if prefix and (name := entity_conf.get(CONF_NAME)) is not None:
            entity_conf[CONF_NAME] = f"{prefix}{name}"
        entity_conf.pop("platform", None)
        entity_conf.pop(CONF_DEVICE_TYPE, None)
        entity_conf.pop(CONF_DEVICE_ADDR, None)
        hub_ref = config[CONF_ID].copy()
        hub_ref.is_declaration = False
        entity_conf[CONF_TOPTRONIC_ID] = hub_ref
        validated = platforms[platform_name](entity_conf)
        _resolve_ids(validated, used_ids)
        generated.append((platform_name, validated))

    _get_data().generated_entities[str(config[CONF_ID])] = generated


def _hub_entities(hub: str, domain: str) -> list[ConfigType]:
    """Config entries of one domain that belong to this hub."""
    from esphome import final_validate as fv

    return [
        entity
        for entity in fv.full_config.get().get(domain, [])
        if entity.get(CONF_PLATFORM) == DOMAIN and str(entity[CONF_TOPTRONIC_ID]) == hub
    ]


def _datapoint_key(entity: ConfigType) -> tuple[int, int, int]:
    return (
        entity[CONF_FUNCTION_GROUP],
        entity[CONF_FUNCTION_NUMBER],
        entity[CONF_DATAPOINT],
    )


def _hub_datapoints(
    hub: str, domains: tuple[str, ...]
) -> list[tuple[tuple[int, int, int], str, str]]:
    """Key, domain and description of this hub's preset and hand-written entries."""
    entries = [
        (_datapoint_key(entity), platform_name, f"preset {platform_name}")
        for platform_name, entity in _get_data().generated_entities.get(hub, [])
        if platform_name in domains
    ]
    for domain in domains:
        entries.extend(
            (_datapoint_key(entity), domain, f"{domain} '{entity.get(CONF_NAME)}'")
            for entity in _hub_entities(hub, domain)
        )
    return entries


def _reject_duplicate_datapoints(config: ConfigType) -> None:
    """Reject two entities of the same direction on one datapoint of this hub.

    A read entity and a write entity may share a datapoint: the write takes its
    state from the read one and does not land in the hub's sensor map. Two read
    entities would share one map slot, so the first would never be polled and
    would stay unknown for good. Two write entities share one input slot, so the
    first would never send its value.
    """
    _reject_duplicates(config, READ_DOMAINS, "read", "would never receive a value")
    _reject_duplicates(config, WRITE_DOMAINS, "written", "would never send a value")


def _reject_duplicates(
    config: ConfigType, domains: tuple[str, ...], verb: str, reason: str
) -> None:
    seen: dict[tuple[int, int, int], str] = {}
    for key, _, source in _hub_datapoints(str(config[CONF_ID]), domains):
        if (first := seen.get(key)) is not None:
            raise cv.Invalid(
                f"Datapoint {key[2]} of function {key[0]}/{key[1]} is already {verb} "
                f"by {first}; {source} {reason}"
            )
        seen[key] = source


def _reject_mismatched_links(config: ConfigType) -> None:
    """Reject a write entity that cannot take its state from the read entity on the same datapoint.

    The hub casts the input to the read entity's class when it links them, so a
    number needs a numeric sensor and a select needs a text sensor on its key.
    """
    hub = str(config[CONF_ID])
    reads = {key: domain for key, domain, _ in _hub_datapoints(hub, READ_DOMAINS)}
    for key, domain, source in _hub_datapoints(hub, tuple(LINKED_DOMAINS)):
        read_domain = LINKED_DOMAINS[domain]
        if (found := reads.get(key)) is not None and found != read_domain:
            raise cv.Invalid(
                f"{source} writes datapoint {key[2]} of function {key[0]}/{key[1]}, "
                f"which is read by a {found}; a {domain} can only take its state from a {read_domain}"
            )


async def _generate_entities(hub, config: ConfigType) -> None:
    """Generate the preset entities validated during final validation."""
    from . import button, number, select, sensor, text_sensor

    codegens = {
        "sensor": sensor.to_code,
        "text_sensor": text_sensor.to_code,
        "number": number.to_code,
        "select": select.to_code,
        "button": button.to_code,
    }
    saved = _get_data().generated_entities.get(str(config[CONF_ID]), [])
    for platform_name, validated in saved:
        await codegens[platform_name](validated)


def _register_refresh_button() -> None:
    """Validate and register the build-wide "Refresh all" button.

    Like the preset entities, its ID is resolved during final validation and
    the validated config is stashed in CORE.data for to_code.
    """
    if (data := _get_data()).refresh_button is not None:
        return
    cfg = button_platform.button_schema(button_platform.ApplyButton)(
        {
            CONF_NAME: "Refresh all",
            CONF_ICON: "mdi:refresh",
            CONF_ENTITY_CATEGORY: "config",
        }
    )
    _resolve_ids(cfg, _shared_used_ids())
    data.refresh_button = cfg


async def _generate_refresh_button(hub) -> None:
    """Emit the "Refresh all" button once, on the first hub; refresh_all() covers every hub."""
    data = _get_data()
    if (cfg := data.refresh_button) is None or data.refresh_button_generated:
        return
    data.refresh_button_generated = True
    buttons = button_platform.sub_buttons({}, parent=hub)
    await buttons.create(cfg, None, apply="refresh_all()")


async def to_code(config: ConfigType) -> None:
    cbus = await cg.get_variable(config[CONF_CANBUS_ID])
    device_id = DEVICE_TYPES[config[CONF_DEVICE_TYPE]] | config[CONF_DEVICE_ADDR]
    var = cg.new_Pvariable(config[CONF_ID], cbus, device_id)
    await cg.register_component(var, config)
    _request_hub()
    # Pause bus traffic while an OTA update runs
    ota.request_ota_state_listeners()
    max_pending = config[CONF_MAX_PENDING_MESSAGES]
    max_frames = config[CONF_MAX_FRAMES_PER_MESSAGE]
    for _ in range(max_pending):
        _request_slot(str(var))
    for _ in range(max_frames):
        _request_frame(str(var))
    cg.add(var.set_max_pending_messages(max_pending))
    cg.add(var.set_max_frames_per_message(max_frames))
    cg.add(var.set_max_pending_age_ms(config[CONF_MAX_PENDING_AGE]))
    cg.add(var.set_write_min_interval_ms(config[CONF_WRITE_MIN_INTERVAL]))
    cg.add(var.set_reject_writes_before_read(config[CONF_REJECT_WRITES_BEFORE_READ]))
    cg.add(var.set_boot_refresh_delay_ms(config[CONF_BOOT_REFRESH_DELAY]))
    cg.add(var.set_refresh_interval_ms(config[CONF_REFRESH_INTERVAL]))
    cg.add(var.set_max_refresh_retries(config[CONF_MAX_REFRESH_RETRIES]))
    cg.add(var.set_refresh_retry_interval_ms(config[CONF_REFRESH_RETRY_INTERVAL]))
    await _generate_entities(var, config)
    await _generate_refresh_button(var)
