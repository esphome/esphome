import logging

import esphome.codegen as cg
from esphome.components import socket
from esphome.components.usb_host import CONF_PID, CONF_VID, USBClient
import esphome.config_validation as cv
from esphome.const import CONF_DISABLED, CONF_ID, CONF_PORT
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@tomaszduda23"]
DEPENDENCIES = ["network", "usb_host"]
DOMAIN = "xerox_phaser_3020"

CONF_JAM_RECOVERY = "jam_recovery"
CONF_LOCATION = "location"
CONF_PAPER = "paper"
CONF_POWER_SAVE = "power_save"
CONF_PRINTER_NAME = "printer_name"
CONF_TONER_DENSITY = "toner_density"
CONF_TONER_SAVE = "toner_save"
CONF_WAIT_FOR_PRINTER = "wait_for_printer"

xerox_phaser_3020_ns = cg.esphome_ns.namespace(DOMAIN)
XeroxPhaser3020 = xerox_phaser_3020_ns.class_("XeroxPhaser3020", USBClient)
TonerSave = xerox_phaser_3020_ns.enum("TonerSave", is_class=True)

# Same order as PAPERS in qpdl.h
PAPERS = {
    "A4": 0,
    "LETTER": 1,
    "LEGAL": 2,
    "EXECUTIVE": 3,
    "A5": 4,
    "A6": 5,
    "B5": 6,
    "ENV10": 7,
    "MONARCH": 8,
    "DL": 9,
    "C5": 10,
}
TONER_DENSITIES = {"LIGHT": 1, "MEDIUM": 3, "DARK": 5}
TONER_SAVE = {
    "PRINTER_DEFAULT": TonerSave.TONER_SAVE_PRINTER_DEFAULT,
    "ON": TonerSave.TONER_SAVE_ON,
    "OFF": TonerSave.TONER_SAVE_OFF,
}
# Power save delays the printer offers, in minutes; 0 turns power save off
POWER_SAVE_MINUTES = (5, 10, 15, 30, 45, 60)

# A job streams on one connection task while the others answer status requests;
# as many connections again can wait in the queue
NUM_WORKERS = 3


def validate_power_save(value: object) -> int:
    if isinstance(value, str) and value.lower() == "off":
        return 0
    minutes = cv.positive_time_period_minutes(value).total_minutes
    if minutes not in POWER_SAVE_MINUTES:
        raise cv.Invalid(
            f"The printer supports a power save delay of {', '.join(f'{m}min' for m in POWER_SAVE_MINUTES)} or off"
        )
    return int(minutes)


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(XeroxPhaser3020),
            # Any USB printer by default; set these to pick one when several are attached
            cv.Optional(CONF_VID, default=0): cv.hex_uint16_t,
            cv.Optional(CONF_PID, default=0): cv.hex_uint16_t,
            cv.Optional(CONF_PRINTER_NAME): cv.string_strict,
            cv.Optional(CONF_LOCATION, default=""): cv.string_strict,
            cv.Optional(CONF_PORT, default=631): cv.port,
            cv.Optional(CONF_PAPER, default="A4"): cv.enum(PAPERS, upper=True),
            cv.Optional(CONF_TONER_DENSITY, default="MEDIUM"): cv.enum(
                TONER_DENSITIES, upper=True
            ),
            cv.Optional(CONF_TONER_SAVE, default="PRINTER_DEFAULT"): cv.enum(
                TONER_SAVE, upper=True, space="_"
            ),
            cv.Optional(CONF_POWER_SAVE, default="5min"): validate_power_save,
            cv.Optional(CONF_JAM_RECOVERY, default=False): cv.boolean,
            cv.Optional(CONF_WAIT_FOR_PRINTER, default="5min"): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(max=cv.TimePeriod(hours=1)),
            ),
        }
    ),
    socket.consume_sockets(1, DOMAIN, socket.SocketType.TCP_LISTEN),
    socket.consume_sockets(2 * NUM_WORKERS, DOMAIN, socket.SocketType.TCP),
)


def _final_validate(config: ConfigType) -> ConfigType:
    if fv.full_config.get().get("mdns", {}).get(CONF_DISABLED):
        _LOGGER.warning(
            "mdns is disabled, so phones and computers will not find the printer by themselves; "
            "it can still be added by its URL"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    # Band, line and dithering buffers for the encoder: in PSRAM when there is some
    work_memory = cg.static_buffer(
        config[CONF_ID],
        cg.uint8,
        xerox_phaser_3020_ns.WORK_MEMORY_SIZE,
        psram=True,
        align=4,
    )
    var = cg.new_Pvariable(
        config[CONF_ID], config[CONF_VID], config[CONF_PID], work_memory
    )
    await cg.register_component(var, config)
    name = config.get(
        CONF_PRINTER_NAME, f"Phaser 3020 @ {CORE.friendly_name or CORE.name}"
    )
    cg.add(var.set_printer_name(name))
    cg.add(var.set_location(config[CONF_LOCATION]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_paper(config[CONF_PAPER]))
    cg.add(var.set_toner_density(config[CONF_TONER_DENSITY]))
    cg.add(var.set_toner_save(config[CONF_TONER_SAVE]))
    cg.add(var.set_power_save_minutes(config[CONF_POWER_SAVE]))
    cg.add(var.set_jam_recovery(config[CONF_JAM_RECOVERY]))
    cg.add(var.set_printer_wait(config[CONF_WAIT_FOR_PRINTER].total_seconds))
