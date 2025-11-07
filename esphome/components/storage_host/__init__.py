from __future__ import annotations

import logging

import esphome.codegen as cg
from esphome import automation
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = []
AUTO_LOAD = []

# Namespaces
storage_host_ns = cg.esphome_ns.namespace("storage_host")

# Classes
StorageHost = storage_host_ns.class_("StorageHost", cg.Component)
FileManager = storage_host_ns.class_("FileManager", cg.Component)

# Triggers
FileAddedTrigger = storage_host_ns.class_("FileAddedTrigger", automation.Trigger)
FileModifiedTrigger = storage_host_ns.class_("FileModifiedTrigger", automation.Trigger)
FileDeletedTrigger = storage_host_ns.class_("FileDeletedTrigger", automation.Trigger)
DirectoryChangedTrigger = storage_host_ns.class_(
    "DirectoryChangedTrigger", automation.Trigger
)

# Info structs
FileInfo = storage_host_ns.struct("FileInfo")
DirectoryChangeInfo = storage_host_ns.struct("DirectoryChangeInfo")

# Configuration keys
CONF_MOUNTS = "mounts"
CONF_MOUNT_PATH = "path"
CONF_MOUNT_PLATFORM = "platform"
CONF_FILE_MANAGERS = "file_managers"
CONF_STORAGE_HOST_ID = "storage_host_id"
CONF_WATCH_DIRECTORY = "watch_directory"
CONF_WATCH_FILE = "watch_file"
CONF_SCAN_INTERVAL = "scan_interval"
CONF_PATTERNS = "patterns"
CONF_MIN_SIZE = "min_size"
CONF_MAX_SIZE = "max_size"
CONF_ON_FILE_ADDED = "on_file_added"
CONF_ON_FILE_MODIFIED = "on_file_modified"
CONF_ON_FILE_DELETED = "on_file_deleted"
CONF_ON_DIRECTORY_CHANGED = "on_directory_changed"

# Platform types
PLATFORM_SD_DIRECT = "sd_direct"
PLATFORM_USB_MSC = "usb_msc"
PLATFORM_SD_MMC = "sd_mmc"
PLATFORM_SPIFFS = "spiffs"
PLATFORM_NFS = "nfs"
PLATFORM_SMB = "smb"

# Single mount configuration
MOUNT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_MOUNT_PATH): cv.string,
        cv.Optional(CONF_MOUNT_PLATFORM, default=PLATFORM_SD_DIRECT): cv.one_of(
            PLATFORM_SD_DIRECT,
            PLATFORM_USB_MSC,
            PLATFORM_SD_MMC,
            PLATFORM_SPIFFS,
            PLATFORM_NFS,
            PLATFORM_SMB,
        ),
    }
)

# File manager configuration with automation triggers
FILE_MANAGER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FileManager),
        cv.GenerateID(CONF_STORAGE_HOST_ID): cv.use_id(StorageHost),
        # Watch either a directory or a single file (mutually exclusive)
        cv.Optional(CONF_WATCH_DIRECTORY): cv.string,
        cv.Optional(CONF_WATCH_FILE): cv.string,
        # Scan interval
        cv.Optional(
            CONF_SCAN_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
        # Pattern filters (glob patterns like "*.jpg", "*.png")
        cv.Optional(CONF_PATTERNS, default=[]): cv.ensure_list(cv.string),
        # Size filters
        cv.Optional(CONF_MIN_SIZE, default=0): cv.int_range(min=0),
        cv.Optional(CONF_MAX_SIZE): cv.int_range(min=0),
        # Automation triggers
        cv.Optional(CONF_ON_FILE_ADDED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FileAddedTrigger),
            }
        ),
        cv.Optional(CONF_ON_FILE_MODIFIED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FileModifiedTrigger),
            }
        ),
        cv.Optional(CONF_ON_FILE_DELETED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FileDeletedTrigger),
            }
        ),
        cv.Optional(CONF_ON_DIRECTORY_CHANGED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DirectoryChangedTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

# StorageHost configuration
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageHost),
        cv.Optional(CONF_MOUNTS, default=[]): cv.ensure_list(MOUNT_SCHEMA),
        cv.Optional(CONF_FILE_MANAGERS, default=[]): cv.ensure_list(
            FILE_MANAGER_SCHEMA
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for storage_host component"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_STORAGE_HOST")

    # Register each mount
    for mount_config in config.get(CONF_MOUNTS, []):
        mount_path = mount_config[CONF_MOUNT_PATH]
        mount_platform = mount_config[CONF_MOUNT_PLATFORM]
        cg.add(var.register_mount(mount_path, mount_platform))

    # Register file managers
    if CONF_FILE_MANAGERS in config:
        for fm_config in config[CONF_FILE_MANAGERS]:
            await setup_file_manager(fm_config, var)


async def setup_file_manager(config, parent_storage):
    """Configure a FileManager component with automation triggers"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Link to parent storage_host component
    storage = await cg.get_variable(config[CONF_STORAGE_HOST_ID])
    cg.add(var.set_storage_host(storage))

    # Set watch directory or file
    if CONF_WATCH_DIRECTORY in config:
        cg.add(var.set_watch_directory(config[CONF_WATCH_DIRECTORY]))
    elif CONF_WATCH_FILE in config:
        cg.add(var.set_watch_file(config[CONF_WATCH_FILE]))
    else:
        raise cv.Invalid("Either watch_directory or watch_file must be specified")

    # Scan interval
    cg.add(var.set_scan_interval(config[CONF_SCAN_INTERVAL]))

    # Pattern filters
    for pattern in config.get(CONF_PATTERNS, []):
        cg.add(var.add_pattern(pattern))

    # Size filters
    if CONF_MIN_SIZE in config:
        cg.add(var.set_min_size(config[CONF_MIN_SIZE]))
    if CONF_MAX_SIZE in config:
        cg.add(var.set_max_size(config[CONF_MAX_SIZE]))

    # Automation triggers
    for conf in config.get(CONF_ON_FILE_ADDED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(FileInfo, "x")], conf)
        cg.add(var.add_on_file_added_callback(trigger))

    for conf in config.get(CONF_ON_FILE_MODIFIED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(FileInfo, "x")], conf)
        cg.add(var.add_on_file_modified_callback(trigger))

    for conf in config.get(CONF_ON_FILE_DELETED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(FileInfo, "x")], conf)
        cg.add(var.add_on_file_deleted_callback(trigger))

    for conf in config.get(CONF_ON_DIRECTORY_CHANGED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(DirectoryChangeInfo, "x")], conf)
        cg.add(var.add_on_directory_changed_callback(trigger))

    return var
