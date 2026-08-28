import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    add_idf_component,
    add_idf_sdkconfig_option,
    idf_version,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEVICES, CONF_ID
from esphome.core import CORE
from esphome.coroutine import CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj
from esphome.cpp_types import Component
from esphome.types import ConfigType

AUTO_LOAD = ["bytebuffer"]
CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["esp32"]
usb_host_ns = cg.esphome_ns.namespace("usb_host")
USBHost = usb_host_ns.class_("USBHost", Component)
USBClient = usb_host_ns.class_("USBClient", Component)
DOMAIN = "usb_host"
CONF_VID = "vid"
CONF_PID = "pid"
CONF_ENABLE_HUBS = "enable_hubs"
CONF_MAX_TRANSFER_REQUESTS = "max_transfer_requests"
CONF_MAX_PACKET_SIZE = "max_packet_size"

# Transfer-class requirement tracking. Consumer components call the require_*()
# functions below; the FINAL-priority job turns the union into defines.
KEY_TRANSFERS_REQUIRED = "transfers_required"
TRANSFER_BULK = "bulk"
TRANSFER_CONTROL = "control"
TRANSFER_ISOC = "isoc"
_TRANSFER_DEFINES = {
    TRANSFER_BULK: "USE_USB_BULK_TRANSFERS",
    TRANSFER_CONTROL: "USE_USB_CONTROL_TRANSFERS",
    TRANSFER_ISOC: "USE_USB_ISOC_TRANSFERS",
}


def usb_device_schema(
    cls=USBClient, vid: int | None = None, pid: int | None = None
) -> cv.Schema:
    schema = cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(cls),
        }
    )
    if vid:
        schema = schema.extend({cv.Optional(CONF_VID, default=vid): cv.hex_uint16_t})
    else:
        schema = schema.extend({cv.Required(CONF_VID): cv.hex_uint16_t})
    if pid:
        schema = schema.extend({cv.Optional(CONF_PID, default=pid): cv.hex_uint16_t})
    else:
        schema = schema.extend({cv.Required(CONF_PID): cv.hex_uint16_t})
    return schema


def _store_host_options(config: dict) -> dict:
    """Publish the host-wide sizing options so consumer components can read them
    from their own validation and codegen."""
    domain_data = CORE.data.setdefault(DOMAIN, {})
    domain_data[CONF_MAX_PACKET_SIZE] = config[CONF_MAX_PACKET_SIZE]
    domain_data[CONF_MAX_TRANSFER_REQUESTS] = config[CONF_MAX_TRANSFER_REQUESTS]
    return config


def _require_transfers(*kinds: str) -> None:
    """Record transfer classes a consumer needs.

    Recording rather than emitting keeps this callable from any consumer at any
    codegen priority, and lets the reconcile job below see the union of every
    request instead of whatever the first caller happened to ask for.
    """
    required = CORE.data.setdefault(DOMAIN, {}).setdefault(
        KEY_TRANSFERS_REQUIRED, set()
    )
    required.update(kinds)


def require_bulk_transfers() -> None:
    """Request the bulk/interrupt transfer API in USBClient.

    A consumer component calls this from its own to_code(). The transfer paths are
    compiled per request so a build only carries the ones some driver actually uses;
    isochronous in particular is dead weight for a serial adapter.
    """
    _require_transfers(TRANSFER_BULK)


def require_control_transfers() -> None:
    """Request the control transfer API, including set_interface()."""
    _require_transfers(TRANSFER_CONTROL)


def require_isoc_transfers() -> None:
    """Request the isochronous stream API.

    Selecting an alt-setting is a control transfer, so isochronous cannot stand on
    its own. The implication is resolved in the reconcile job rather than here, so
    a consumer only has to state what it actually uses.
    """
    _require_transfers(TRANSFER_ISOC)


@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_transfer_defines() -> None:
    """Emit the transfer-class defines once, after every require_*() call.

    Consumer to_code() runs at a higher priority than FINAL, so by the time this
    job runs every request has been recorded. Reading the set inline from
    usb_host's own to_code() instead would depend on component iteration order and
    would silently drop the requests of any consumer that had not run yet.
    """
    required = set(CORE.data.get(DOMAIN, {}).get(KEY_TRANSFERS_REQUIRED, ()))
    if TRANSFER_ISOC in required:
        # Alt-setting selection is a control transfer, so isochronous cannot stand
        # alone; usb_host.h enforces the same dependency with an #error for anyone
        # defining the macros by hand.
        required.add(TRANSFER_CONTROL)
    for kind in sorted(required):
        cg.add_define(_TRANSFER_DEFINES[kind])


def get_max_packet_size() -> int:
    return CORE.data.get(DOMAIN, {}).get(CONF_MAX_PACKET_SIZE, 64)


def get_max_transfer_requests() -> int:
    return CORE.data.get(DOMAIN, {}).get(CONF_MAX_TRANSFER_REQUESTS, 16)


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBHost),
            cv.Optional(CONF_ENABLE_HUBS, default=False): cv.boolean,
            cv.Optional(CONF_MAX_TRANSFER_REQUESTS, default=16): cv.int_range(
                min=1, max=32
            ),
            cv.Optional(CONF_MAX_PACKET_SIZE, default=64): cv.one_of(
                64, 128, 256, 512, 1024, int=True
            ),
            cv.Optional(CONF_DEVICES): cv.ensure_list(usb_device_schema()),
        }
    ),
    only_on_variant(
        supported=[
            VARIANT_ESP32H4,
            VARIANT_ESP32P4,
            VARIANT_ESP32S2,
            VARIANT_ESP32S3,
            VARIANT_ESP32S31,
        ]
    ),
    _store_host_options,
)


async def register_usb_client(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_VID], config[CONF_PID])
    await cg.register_component(var, config)
    return var


async def to_code(config: ConfigType) -> None:
    # IDF 6.0 moved USB host to an external component; 1.4.1 requires IDF >= 6.0
    if idf_version() >= cv.Version(6, 0, 0):
        add_idf_component(name="espressif/usb", ref="1.4.1")

    add_idf_sdkconfig_option("CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE", 1024)
    if config.get(CONF_ENABLE_HUBS):
        add_idf_sdkconfig_option("CONFIG_USB_HOST_HUBS_SUPPORTED", True)

    cg.add_define("USB_HOST_MAX_REQUESTS", config[CONF_MAX_TRANSFER_REQUESTS])
    cg.add_define("USB_HOST_MAX_PACKET_SIZE", config[CONF_MAX_PACKET_SIZE])

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    devices = config.get(CONF_DEVICES)
    if devices:
        # A bare devices: entry has no driver component behind it to request transfer
        # classes, so it is only useful through lambdas. Give it the standard pair rather
        # than a USBClient that cannot transfer anything at all.
        require_bulk_transfers()
        require_control_transfers()

    # FINAL: require_*() calls arrive from consumer to_code() at higher priorities, so
    # turn the collected set into defines once after every job ran. Emitting per call
    # instead would make the result depend on component iteration order.
    CORE.add_job(_emit_transfer_defines)

    for device in devices or ():
        await register_usb_client(device)
