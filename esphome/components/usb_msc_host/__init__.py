from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_component,
    only_on_variant,
    require_vfs_dir,
)
from esphome.components.usb_host import USBHost, usb_host_ns
import esphome.config_validation as cv
from esphome.const import CONF_DEVICES, CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = ["p1ngb4ck"]
DEPENDENCIES = ["usb_host", "esp32"]
AUTO_LOAD = []

CONF_USB_HOST_ID = "usb_host_id"
CONF_MOUNT_PATH = "mount_path"
CONF_VID = "vid"
CONF_PID = "pid"
CONF_ON_MOUNTED = "on_mounted"

require_vfs_dir()

usb_msc_host_ns = cg.esphome_ns.namespace("usb_msc_host")
USBMscHost = usb_msc_host_ns.class_("USBMscHost", cg.Component)
USBMscDevice = usb_msc_host_ns.class_(
    "USBMscDevice",
    cg.Component,
    usb_host_ns.class_("USBDeviceHandler"),
    cg.Parented.template(USBMscHost),
)

# Automation classes
DeviceMountedTrigger = usb_msc_host_ns.class_(
    "DeviceMountedTrigger", automation.Trigger.template(cg.std_string)
)
RemountDeviceAction = usb_msc_host_ns.class_("RemountDeviceAction", automation.Action)
UnmountDeviceAction = usb_msc_host_ns.class_("UnmountDeviceAction", automation.Action)
ListFilesAction = usb_msc_host_ns.class_("ListFilesAction", automation.Action)
DeviceMountedCondition = usb_msc_host_ns.class_(
    "DeviceMountedCondition", automation.Condition
)


async def register_usb_msc_handler(device_config, msc_host, usb_host):
    # Interface-class based handler with optional VID/PID filtering
    var = cg.new_Pvariable(device_config[CONF_ID])
    await cg.register_component(var, device_config)
    cg.add(var.set_parent(msc_host))  # Set USBMscHost as parent
    cg.add(
        var.set_usb_host(usb_host)
    )  # Set USBHost reference for closing device handles

    # Set mount path
    cg.add(var.set_mount_path(device_config[CONF_MOUNT_PATH]))

    # Set VID/PID (0x0000 means wildcard - match any)
    cg.add(var.set_vid(device_config[CONF_VID]))
    cg.add(var.set_pid(device_config[CONF_PID]))

    cg.add(
        usb_host.register_device_handler(var)
    )  # Register as interface-class handler with USBHost

    # Register on_mounted trigger
    for conf in device_config.get(CONF_ON_MOUNTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "mount_path")], conf
        )

    return var


DEVICE_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(USBMscDevice),
        cv.Required(CONF_MOUNT_PATH): cv.string,
        cv.Optional(CONF_VID, default=0x0000): cv.hex_uint16_t,
        cv.Optional(CONF_PID, default=0x0000): cv.hex_uint16_t,
        cv.Optional(CONF_ON_MOUNTED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DeviceMountedTrigger),
            }
        ),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBMscHost),
            cv.GenerateID(CONF_USB_HOST_ID): cv.use_id(USBHost),
            cv.Optional(CONF_DEVICES): cv.ensure_list(DEVICE_SCHEMA),
        }
    ),
    cv.only_with_esp_idf,
    only_on_variant(supported=[VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4]),
)


async def to_code(config):
    add_idf_component(name="espressif/usb_host_msc", ref="1.1.4")

    # Get USBHost instance for handler registration
    usb_host_var = await cg.get_variable(config[CONF_USB_HOST_ID])

    # Create USBMscHost
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Register interface-class based handlers and store them for later use
    # by storage_host to register mount callbacks
    for device in config.get(CONF_DEVICES) or ():
        device_var = await register_usb_msc_handler(device, var, usb_host_var)
        # Store device reference in CORE.data for storage_host to access
        # This allows storage_host to register callbacks with USB MSC devices
        from esphome.core import CORE

        if not hasattr(CORE, "data"):
            CORE.data = {}
        if "usb_msc_devices" not in CORE.data:
            CORE.data["usb_msc_devices"] = []
        CORE.data["usb_msc_devices"].append(device_var)


# Actions
USB_MSC_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(USBMscDevice),
    }
)


@automation.register_action(
    "usb_msc_host.remount", RemountDeviceAction, USB_MSC_ACTION_SCHEMA
)
async def usb_msc_remount_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "usb_msc_host.unmount", UnmountDeviceAction, USB_MSC_ACTION_SCHEMA
)
async def usb_msc_unmount_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "usb_msc_host.list_files", ListFilesAction, USB_MSC_ACTION_SCHEMA
)
async def usb_msc_list_files_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


# Conditions
@automation.register_condition(
    "usb_msc_host.is_mounted", DeviceMountedCondition, USB_MSC_ACTION_SCHEMA
)
async def usb_msc_is_mounted_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
