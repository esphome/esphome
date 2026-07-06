import esphome.codegen as cg
from esphome.components.storage import request_storage_device, request_storage_worker
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_UID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RTL87XX,
)

CODEOWNERS = ["@p1ngb4ck"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["storage"]

CONF_SERVER = "server"
CONF_EXPORT = "export"
CONF_MOUNT_PATH = "mount_path"
CONF_GID = "gid"

nfs_client_ns = cg.esphome_ns.namespace("nfs_client")
NFSClient = nfs_client_ns.class_("NFSClient", cg.Component)

DEFAULT_PORT = 2049
DEFAULT_UID = 0
DEFAULT_GID = 0

NFS_SHARE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NFSClient),
        cv.Required(CONF_SERVER): cv.string,
        cv.Required(CONF_EXPORT): cv.string,
        cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        cv.Optional(CONF_MOUNT_PATH): cv.string,
        cv.Optional(CONF_UID, default=DEFAULT_UID): cv.int_range(min=0, max=65535),
        cv.Optional(CONF_GID, default=DEFAULT_GID): cv.int_range(min=0, max=65535),
    }
).extend(cv.COMPONENT_SCHEMA)


def validate_mount_paths(configs):
    mount_paths = []
    for config in configs:
        if CONF_MOUNT_PATH in config:
            path = config[CONF_MOUNT_PATH]
            if path in mount_paths:
                raise cv.Invalid(
                    f"Duplicate mount path '{path}'. Each NFS share must have a unique mount path."
                )
            mount_paths.append(path)
    return configs


CONFIG_SCHEMA = cv.All(
    cv.ensure_list(NFS_SHARE_SCHEMA),
    validate_mount_paths,
    cv.only_on(
        [
            PLATFORM_BK72XX,
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_LN882X,
            PLATFORM_RTL87XX,
        ]
    ),
)


async def to_code(config):
    for share_config in config:
        var = cg.new_Pvariable(share_config[CONF_ID])
        await cg.register_component(var, share_config)

        cg.add(var.set_server(share_config[CONF_SERVER]))
        cg.add(var.set_port(share_config[CONF_PORT]))
        cg.add(var.set_export(share_config[CONF_EXPORT]))
        cg.add(var.set_uid(share_config[CONF_UID]))
        cg.add(var.set_gid(share_config[CONF_GID]))

        if (mount_path := share_config.get(CONF_MOUNT_PATH)) is not None:
            cg.add(var.set_mount_path(mount_path))

        request_storage_device()
        request_storage_worker(task_safe=True)
