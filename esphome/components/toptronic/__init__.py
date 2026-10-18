from enum import Enum
import pathlib

import yaml

import esphome.codegen as cg
from esphome.components.canbus import CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, ID
from esphome.cpp_types import Component

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["canbus"]
DOMAIN = "toptronic"
AUTO_LOAD = ["sensor", "number", "select", "text_sensor"]
MULTI_CONF = True

CONF_TOPTRONIC_ID = "toptronic_id"
CONF_CANBUS_ID = "canbus_id"
CONF_DEVICE_ADDR = "device_addr"
CONF_FUNCTION_GROUP = "function_group"
CONF_FUNCTION_NUMBER = "function_number"
CONF_DATAPOINT = "datapoint"

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)
TopTronicBase = toptronic.class_("TopTronicBase", cg.PollingComponent)

TT_TYPE = toptronic.enum("TypeName")
_TT_TYPES = ("U8", "U16", "U32", "S8", "S16", "S32", "S64")
TT_TYPE_OPTIONS = {name: getattr(TT_TYPE, name) for name in _TT_TYPES}


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


_device_types = {t.name: t.value for t in DeviceType}

PRESETS_DIR = pathlib.Path(__file__).parent / "presets"
_IDS_KEY = "toptronic_used_ids"


def _shared_used_ids():
    if _IDS_KEY not in CORE.data:
        CORE.data[_IDS_KEY] = set()
    return CORE.data[_IDS_KEY]


def get_device_type(t: str) -> int:
    if t not in _device_types:
        raise ValueError(f'device type "{t}" not found')
    return _device_types.get(t)


def _validate_preset(config):
    device_type = config["device_type"]
    if device_type not in _device_types:
        raise cv.Invalid(
            f"Device type '{device_type}' is not a known TopTronic device type"
        )
    if not (PRESETS_DIR / device_type).is_dir():
        raise cv.Invalid(f"No preset directory found for device type '{device_type}'")
    return config


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Required(CONF_FUNCTION_GROUP): cv.uint8_t,
        cv.Required(CONF_FUNCTION_NUMBER): cv.uint8_t,
        cv.Required(CONF_DATAPOINT): cv.uint16_t,
    }
)


def config_schema_polling(update_interval: str = "30s"):
    return CONFIG_SCHEMA_BASE.extend(cv.polling_component_schema(update_interval))


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TopTronicComponent),
            cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
            cv.Required("device_type"): cv.one_of(
                *[t.name for t in DeviceType], upper=True
            ),
            cv.Required(CONF_DEVICE_ADDR): cv.uint8_t,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_preset,
)


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
    # Preset entities bypass config.py's ID pass; resolve nested IDs here.
    if used is None:
        used = set()
    if isinstance(obj, ID):
        if obj.id is None:
            obj.resolve(used)
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


async def _generate_entities(hub, config):
    from . import sensor

    platforms = {"sensor": (sensor.CONFIG_SCHEMA, sensor.to_code)}
    used_ids = _shared_used_ids()
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
        schema, codegen = platforms[platform_name]
        validated = schema(entity_conf)
        _resolve_ids(validated, used_ids)
        await codegen(validated)


async def to_code(config):
    cbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], cbus)
    await cg.register_component(var, config)
    cg.add(var.set_device_type(get_device_type(config["device_type"])))
    cg.add(var.set_device_addr(config[CONF_DEVICE_ADDR]))
    await _generate_entities(var, config)
    cg.add(var.configure_hub())
