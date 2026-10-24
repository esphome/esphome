from enum import Enum

import esphome.codegen as cg
from esphome.components.canbus import CanbusComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["canbus"]
DOMAIN = "toptronic"
MULTI_CONF = True

CONF_CANBUS_ID = "canbus_id"
CONF_DEVICE_ADDR = "device_addr"

toptronic = cg.esphome_ns.namespace("toptronic")
TopTronicComponent = toptronic.class_("TopTronic", cg.Component)


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


def get_device_type(t: str) -> int:
    if t not in _device_types:
        raise ValueError(f'device type "{t}" not found')
    return _device_types[t]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TopTronicComponent),
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Required("device_type"): cv.one_of(*DeviceType.__members__, upper=True),
        cv.Required(CONF_DEVICE_ADDR): cv.uint8_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    cbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], cbus)
    await cg.register_component(var, config)
    cg.add(var.set_device_type(get_device_type(config["device_type"])))
    cg.add(var.set_device_addr(config[CONF_DEVICE_ADDR]))
