from dataclasses import dataclass, field
from enum import Enum
import pathlib

import yaml

import esphome.codegen as cg
from esphome.components import ota
from esphome.components.canbus import CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OPTIONS
from esphome.core import CORE, ID
from esphome.cpp_types import Component
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["canbus"]
DOMAIN = "toptronic"
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

CONF_TOPTRONIC_ID = "toptronic_id"
CONF_CANBUS_ID = "canbus_id"
CONF_DEVICE_ADDR = "device_addr"
CONF_FUNCTION_GROUP = "function_group"
CONF_FUNCTION_NUMBER = "function_number"
CONF_DATAPOINT = "datapoint"
CONF_DECIMAL = "decimal"
CONF_VALUES = "values"
CONF_BOOT_REFRESH_DELAY = "boot_refresh_delay"
CONF_MAX_PENDING_MESSAGES = "max_pending_messages"
CONF_MAX_PENDING_AGE = "max_pending_age"
CONF_CLEANUP_INTERVAL = "cleanup_interval"
CONF_MAX_REFRESH_PER_LOOP = "max_refresh_per_loop"
CONF_MAX_FRAMES_PER_MESSAGE = "max_frames_per_message"
CONF_REFRESH_GAP = "refresh_gap"
CONF_MAX_REFRESH_RETRIES = "max_refresh_retries"
CONF_REFRESH_RETRY_INTERVAL = "refresh_retry_interval"
CONF_WRITE_MIN_INTERVAL = "write_min_interval"
CONF_REJECT_WRITES_BEFORE_READ = "reject_writes_before_read"

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)
TopTronicBase = toptronic.class_("TopTronicBase", cg.PollingComponent)

TT_TYPE = toptronic.enum("TypeName")
TT_TYPE_OPTIONS = {
    "U8": TT_TYPE.U8,
    "U16": TT_TYPE.U16,
    "U32": TT_TYPE.U32,
    "S8": TT_TYPE.S8,
    "S16": TT_TYPE.S16,
    "S32": TT_TYPE.S32,
    "S64": TT_TYPE.S64,
}


class DeviceType(Enum):
    WEZ = 0  # EN: Heat generator / FR: Générateur de chaleur / DE: Wärmeerzeuger
    SOL = 64  # EN: Solar module / FR: Module solaire / DE: Solar
    PS = 128  # EN: Buffer storage tank / FR: Ballon tampon / DE: Pufferspeicher
    FW = 192  # EN: District heating / FR: Chauffage urbain / DE: Fernwärme
    HK = 256  # EN: Heating circuit / FR: Circuit de chauffage / DE: Heizkreis
    MWA = 384  # EN: Energy meter module / FR: Module de mesure d'énergie / DE: Messwertauswertung
    GLT = 448  # EN: Building mgmt system (BMS) / FR: Gestion technique du bâtiment (GTB) / DE: Gebäudeleittechnik
    HV = 512  # EN: HomeVent ventilation / FR: Ventilation HomeVent / DE: HomeVent
    BM = 1024  # EN: Control module (Display) / FR: Module de commande (Écran) / DE: Bedienmodul
    BD = BM  # EN: Control display (Alias) / FR: Écran de commande (Alias) / DE: Bediendisplay
    GW = 1153  # EN: Gateway (Modbus/KNX) / FR: Passerelle (Modbus/KNX) / DE: Gateway


_device_types = {name: t.value for name, t in DeviceType.__members__.items()}

PRESETS_DIR = pathlib.Path(__file__).parent / "presets"

_PRESETS_KEY = "toptronic_used_ids"
_PRESET_ENTITIES_KEY = "toptronic_generated_entities"


@dataclass
class TopTronicData:
    """Per-run state shared between final validation and code generation."""

    # Validated preset entity configs per hub ID (built in FINAL_VALIDATE_SCHEMA
    # so their Component IDs are recorded early enough to count into
    # ESPHOME_COMPONENT_COUNT).
    generated_entities: dict[str, list[tuple[str, ConfigType]]] = field(
        default_factory=dict
    )


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


def get_device_type(t: str) -> int:
    if t not in _device_types:
        raise ValueError(f'device type "{t}" not found')
    return _device_types.get(t)


def _validate_options_values_lengths(config):
    """Validate that 'options' and 'values' lists have matching lengths (select/text_sensor).

    Per instructions.md §7.3 these MUST match; a mismatch would otherwise surface as a
    bare IndexError during code generation instead of a clean validation error.
    """
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
    """Return CONFIG_SCHEMA_BASE extended with a configurable polling interval.

    Read-only entities (sensor/text_sensor) poll the bus via their inherited
    PollingComponent update() at this interval. Write-only entities (number,
    select, button) use plain CONFIG_SCHEMA_BASE instead — they have no update
    callback, so polling them every interval would only wake the scheduler for a
    no-op. Without the polling schema, register_component() does not emit
    set_update_interval(), leaving the PollingComponent default SCHEDULER_DONT_RUN.
    """
    return CONFIG_SCHEMA_BASE.extend(cv.polling_component_schema(update_interval))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TopTronicComponent),
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Required("device_type"): cv.one_of(*DeviceType.__members__, upper=True),
        cv.Required(CONF_DEVICE_ADDR): cv.uint8_t,
        cv.Optional(
            CONF_BOOT_REFRESH_DELAY, default="30s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_PENDING_MESSAGES, default=32): cv.int_range(
            min=1, max=512
        ),
        cv.Optional(
            CONF_MAX_PENDING_AGE, default="5000ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_CLEANUP_INTERVAL, default="5000ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_REFRESH_PER_LOOP, default=8): cv.int_range(min=1, max=255),
        cv.Optional(CONF_MAX_FRAMES_PER_MESSAGE, default=8): cv.int_range(
            min=3, max=31
        ),
        cv.Optional(
            CONF_REFRESH_GAP, default="50ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_REFRESH_RETRIES, default=1): cv.int_range(min=0, max=10),
        cv.Optional(
            CONF_REFRESH_RETRY_INTERVAL, default="200ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_WRITE_MIN_INTERVAL, default="2s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_REJECT_WRITES_BEFORE_READ, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config: ConfigType) -> None:
    """Generate and validate this hub's preset entities at validation time."""
    _register_preset_entities(config)


FINAL_VALIDATE_SCHEMA = _final_validate


def _load_entities(device_type: str):
    entities = []
    for kind in ("sensors", "inputs", "buttons"):
        path = PRESETS_DIR / device_type / f"{kind}_en.yaml"
        if not path.exists():
            continue
        with pathlib.Path(path).open(encoding="utf-8") as f:
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
    from . import number, select, sensor, text_sensor

    platforms = {
        "sensor": sensor.CONFIG_SCHEMA,
        "text_sensor": text_sensor.CONFIG_SCHEMA,
        "number": number.CONFIG_SCHEMA,
        "select": select.CONFIG_SCHEMA,
    }
    used_ids = _shared_used_ids()
    generated = []
    for platform_name, entity_conf in _load_entities(config["device_type"]):
        if platform_name not in platforms:
            raise cv.Invalid(
                f"Unsupported platform '{platform_name}' in toptronic preset"
            )
        entity_conf.pop("platform", None)
        entity_conf.pop("device_type", None)
        entity_conf.pop(CONF_DEVICE_ADDR, None)
        hub_ref = config[CONF_ID].copy()
        hub_ref.is_declaration = False
        entity_conf[CONF_TOPTRONIC_ID] = hub_ref

        validated = platforms[platform_name](entity_conf)
        _resolve_ids(validated, used_ids)
        generated.append((platform_name, validated))

    _get_data().generated_entities[str(config[CONF_ID])] = generated


async def _generate_entities(hub, config: ConfigType) -> None:
    """Generate the preset entities validated during final validation."""
    from . import number, select, sensor, text_sensor

    codegens = {
        "sensor": sensor.to_code,
        "text_sensor": text_sensor.to_code,
        "number": number.to_code,
        "select": select.to_code,
    }
    saved = _get_data().generated_entities.get(str(config[CONF_ID]), [])
    for platform_name, validated in saved:
        await codegens[platform_name](validated)


async def to_code(config):
    cbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], cbus)
    await cg.register_component(var, config)

    # Pause CAN processing while an OTA update runs.
    ota.request_ota_state_listeners()

    device_type = get_device_type(config["device_type"])
    cg.add(var.set_device_type(device_type))
    cg.add(var.set_device_addr(config[CONF_DEVICE_ADDR]))
    cg.add(var.set_boot_refresh_delay(config[CONF_BOOT_REFRESH_DELAY]))
    cg.add(var.set_max_pending_messages(config[CONF_MAX_PENDING_MESSAGES]))
    cg.add(var.set_max_pending_age_ms(config[CONF_MAX_PENDING_AGE]))
    cg.add(var.set_cleanup_interval_ms(config[CONF_CLEANUP_INTERVAL]))
    cg.add(var.set_max_refresh_per_loop(config[CONF_MAX_REFRESH_PER_LOOP]))
    cg.add(var.set_max_frames_per_message(config[CONF_MAX_FRAMES_PER_MESSAGE]))
    cg.add(var.set_refresh_gap_ms(config[CONF_REFRESH_GAP]))
    cg.add(var.set_max_refresh_retries(config[CONF_MAX_REFRESH_RETRIES]))
    cg.add(var.set_refresh_retry_interval_ms(config[CONF_REFRESH_RETRY_INTERVAL]))
    cg.add(var.set_write_min_interval(config[CONF_WRITE_MIN_INTERVAL]))
    cg.add(var.set_reject_writes_before_read(config[CONF_REJECT_WRITES_BEFORE_READ]))

    await _generate_entities(var, config)
