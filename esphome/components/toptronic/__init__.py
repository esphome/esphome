import pathlib
from enum import Enum

import yaml

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button as button_platform
from esphome.components.canbus import CanbusComponent
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_NAME,
    CONF_OPTIONS,
    CONF_SUBSTITUTIONS,
)
from esphome.core import CORE, ID
from esphome.cpp_types import Component

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["canbus"]
AUTO_LOAD = ["sensor", "number", "select", "text_sensor", "button", "switch"]
MULTI_CONF = True

CONF_TT_ID = "toptronic_id"
CONF_CANBUS_ID = "canbus_id"
CONF_DEVICE_TYPE = "device_type"
CONF_DEVICE_ADDR = "device_addr"
CONF_FUNCTION_GROUP = "function_group"
CONF_FUNCTION_NUMBER = "function_number"
CONF_DATAPOINT = "datapoint"
CONF_DECIMAL = "decimal"
CONF_VALUES = "values"
CONF_LANGUAGE = "language"
CONF_BOOT_REFRESH_DELAY = "boot_refresh_delay"
CONF_MAX_PENDING_MESSAGES = "max_pending_messages"
CONF_MAX_PENDING_AGE = "max_pending_age"
CONF_CLEANUP_INTERVAL = "cleanup_interval"
CONF_MAX_REFRESH_PER_LOOP = "max_refresh_per_loop"
CONF_MAX_FRAMES_PER_MESSAGE = "max_frames_per_message"
CONF_REFRESH_GAP_MS = "refresh_gap_ms"
CONF_MAX_REFRESH_RETRIES = "max_refresh_retries"
CONF_REFRESH_RETRY_INTERVAL_MS = "refresh_retry_interval_ms"

LANGS = ("de", "en", "fr", "it")

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)

TopTronicBase = toptronic.class_("TopTronicBase", cg.PollingComponent)

# Auto-generated "Refresh all" button (one per build). press_action() calls the
# parent hub's refresh_all(), which fans out to every registered hub.
TopTronicRefreshButton = toptronic.class_(
    "TopTronicRefreshButton", button_platform.Button, cg.Component
)

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
    WEZ = 0     # EN: Heat generator / FR: Générateur de chaleur / DE: Wärmeerzeuger
    SOL = 64    # EN: Solar module / FR: Module solaire / DE: Solar
    PS = 128    # EN: Buffer storage tank / FR: Ballon tampon / DE: Pufferspeicher
    FW = 192    # EN: District heating / FR: Chauffage urbain / DE: Fernwärme
    HK = 256    # EN: Heating circuit / FR: Circuit de chauffage / DE: Heizkreis
    MWA = 384   # EN: Energy meter module / FR: Module de mesure d'énergie / DE: Messwertauswertung
    GLT = 448   # EN: Building mgmt system (BMS) / FR: Gestion technique du bâtiment (GTB) / DE: Gebäudeleittechnik
    HV = 512    # EN: HomeVent ventilation / FR: Ventilation HomeVent / DE: HomeVent
    BM = 1024   # EN: Control module (Display) / FR: Module de commande (Écran) / DE: Bedienmodul
    BD = 1024   # EN: Control display (Alias) / FR: Écran de commande (Alias) / DE: Bediendisplay
    GW = 1153   # EN: Gateway (Modbus/KNX) / FR: Passerelle (Modbus/KNX) / DE: Gateway


_device_types = {t.name: t.value for t in DeviceType}

PRESETS_DIR = pathlib.Path(__file__).parent / "presets"

_IDS_KEY = "toptronic_used_ids"


def _shared_used_ids():
    """Build-wide set of used IDs, shared across all hub instances."""
    if _IDS_KEY not in CORE.data:
        CORE.data[_IDS_KEY] = set()
    return CORE.data[_IDS_KEY]


def get_device_type(t: str) -> int:
    if t not in _device_types:
        raise ValueError(f'device type "{t}" not found')
    return _device_types.get(t)


def _validate_preset(config):
    device_type = config[CONF_DEVICE_TYPE]
    if device_type not in _device_types:
        raise cv.Invalid(f"Device type '{device_type}' is not a known TopTronic device type")
    if not (PRESETS_DIR / device_type).is_dir():
        available = sorted(d.name for d in PRESETS_DIR.iterdir() if d.is_dir()) if PRESETS_DIR.is_dir() else []
        raise cv.Invalid(
            f"No preset directory found for device type '{device_type}'. "
            f"Available presets: {', '.join(available)}"
        )
    return config


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


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TopTronicComponent),
            cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
            cv.Required(CONF_DEVICE_TYPE): cv.one_of(
                *[t.name for t in DeviceType], upper=True
            ),
            cv.Required(CONF_DEVICE_ADDR): cv.uint8_t,
            cv.Optional(CONF_LANGUAGE, default="en"): cv.one_of(*LANGS, lower=True),
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
            cv.Optional(CONF_MAX_REFRESH_PER_LOOP, default=8): cv.int_range(
                min=1, max=255
            ),
            cv.Optional(CONF_MAX_FRAMES_PER_MESSAGE, default=8): cv.int_range(
                min=3, max=31
            ),
            cv.Optional(
                CONF_REFRESH_GAP_MS, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_REFRESH_RETRIES, default=1): cv.int_range(
                min=0, max=10
            ),
            cv.Optional(
                CONF_REFRESH_RETRY_INTERVAL_MS, default="200ms"
            ): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_preset,
)


def _load_entities(device_type: str, language: str):
    entities = []
    for kind in ("sensors", "inputs", "buttons"):
        path = PRESETS_DIR / device_type / f"{kind}_{language}.yaml"
        if not path.exists():
            continue
        with open(path, encoding="utf-8") as f:
            data = yaml.safe_load(f) or {}
        for platform_name, entries in data.items():
            for entry in entries or []:
                entities.append((platform_name, dict(entry)))
    return entities


def _resolve_ids(obj, used=None):
    """Resolve auto-generated IDs inside a validated entity config.

    Entities synthesized from presets bypass config.py's global ID pass, so
    nested IDs (e.g. filter IDs) keep id=None. Resolve them now to avoid
    duplicate empty-ID registrations during code generation, and record
    Component-derived declarations in CORE.component_ids as config.py would.
    """
    if used is None:
        used = set()
    if isinstance(obj, ID):
        if obj.id is None:
            obj.resolve(used)
        used.add(obj.id)
        if obj.is_declaration and isinstance(obj.type, cg.MockObjClass) and obj.type.inherits_from(
            Component
        ):
            CORE.component_ids.add(obj.id)
    elif isinstance(obj, dict):
        for value in obj.values():
            _resolve_ids(value, used)
    elif isinstance(obj, (list, tuple)):
        for value in obj:
            _resolve_ids(value, used)
    return used


async def _generate_entities(hub, config):
    from . import button, number, select, sensor, text_sensor

    platforms = {
        "sensor": (sensor.CONFIG_SCHEMA, sensor.to_code),
        "text_sensor": (text_sensor.CONFIG_SCHEMA, text_sensor.to_code),
        "number": (number.CONFIG_SCHEMA, number.to_code),
        "select": (select.CONFIG_SCHEMA, select.to_code),
        "button": (button.CONFIG_SCHEMA, button.to_code),
    }

    used_ids = _shared_used_ids()
    for platform_name, entity_conf in _load_entities(
        config[CONF_DEVICE_TYPE], config[CONF_LANGUAGE]
    ):
        if platform_name not in platforms:
            raise cv.Invalid(
                f"Unsupported platform '{platform_name}' in toptronic preset"
            )
        entity_conf.pop("platform", None)
        entity_conf.pop(CONF_DEVICE_TYPE, None)
        entity_conf.pop(CONF_DEVICE_ADDR, None)
        hub_ref = config[CONF_ID].copy()
        hub_ref.is_declaration = False
        entity_conf[CONF_TT_ID] = hub_ref

        schema, codegen = platforms[platform_name]
        validated = schema(entity_conf)
        _resolve_ids(validated, used_ids)
        await codegen(validated)


_REFRESH_BUTTON_KEY = "toptronic_refresh_button_generated"


async def _generate_refresh_button(hub_var):
    """Emit a single build-wide 'Refresh all' button (once, on the first hub).

    The on-press action is handled in C++ (TopTronicRefreshButton::press_action()
    -> refresh_all()), so no lambda or hardcoded hub id is needed in YAML. The
    button id is auto-generated from the component namespace.
    """
    if CORE.data.setdefault(_REFRESH_BUTTON_KEY, False):
        return
    CORE.data[_REFRESH_BUTTON_KEY] = True

    friendly = (CORE.config.get(CONF_SUBSTITUTIONS, {}) or {}).get("friendly_name", "") or ""
    name = f"{friendly} Refresh all" if friendly else "Refresh all"

    cfg = button_platform.button_schema(TopTronicRefreshButton)(
        {
            CONF_NAME: name,
            CONF_ICON: "mdi:refresh",
            CONF_ENTITY_CATEGORY: "config",
        }
    )
    _resolve_ids(cfg, _shared_used_ids())
    var = cg.new_Pvariable(cfg[CONF_ID])
    cg.add(var.set_parent(hub_var))
    await button_platform.register_button(var, cfg)
    await cg.register_component(var, cfg)


async def to_code(config):
    cbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], cbus)
    await cg.register_component(var, config)

    device_type = get_device_type(config[CONF_DEVICE_TYPE])
    cg.add(var.set_device_type(device_type))
    cg.add(var.set_device_addr(config[CONF_DEVICE_ADDR]))
    cg.add(var.set_boot_refresh_delay(config[CONF_BOOT_REFRESH_DELAY]))
    cg.add(var.set_max_pending_messages(config[CONF_MAX_PENDING_MESSAGES]))
    cg.add(var.set_max_pending_age_ms(config[CONF_MAX_PENDING_AGE]))
    cg.add(var.set_cleanup_interval_ms(config[CONF_CLEANUP_INTERVAL]))
    cg.add(var.set_max_refresh_per_loop(config[CONF_MAX_REFRESH_PER_LOOP]))
    cg.add(var.set_max_frames_per_message(config[CONF_MAX_FRAMES_PER_MESSAGE]))
    cg.add(var.set_refresh_gap_ms(config[CONF_REFRESH_GAP_MS]))
    cg.add(var.set_max_refresh_retries(config[CONF_MAX_REFRESH_RETRIES]))
    cg.add(var.set_refresh_retry_interval_ms(config[CONF_REFRESH_RETRY_INTERVAL_MS]))

    await _generate_entities(var, config)

    await _generate_refresh_button(var)

    # Critical wiring (update callbacks, command queue, pump, boot-refresh gate)
    # must not depend on ESPHome invoking setup(): ESPHOME_COMPONENT_COUNT is
    # computed from CORE.component_ids before preset entities are registered, so
    # a hub can be silently dropped from components_ and setup() never runs.
    # Config-phase statements always run for every hub.
    cg.add(var.configure_hub())
