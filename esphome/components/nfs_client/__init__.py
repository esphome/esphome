import esphome.codegen as cg
from esphome.components.storage import (
    MountableStorage,
    register_mount_path,
    request_path_length,
    request_storage_device,
    request_storage_worker,
    validate_mount_path,
)
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
CONF_AUTO_CONNECT = "auto_connect"

nfs_client_ns = cg.esphome_ns.namespace("nfs_client")
# MountableStorage parent makes nfs ids valid targets for the generic storage.mount /
# storage.unmount actions (cv.use_id(MountableStorage) checks declared Python parents).
NFSClient = nfs_client_ns.class_("NFSClient", cg.Component, MountableStorage)

DEFAULT_PORT = 2049
DEFAULT_UID = 0
DEFAULT_GID = 0

NFS_SHARE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NFSClient),
        cv.Required(CONF_SERVER): cv.string,
        cv.Required(CONF_EXPORT): cv.string,
        cv.Optional(CONF_PORT, default=DEFAULT_PORT): cv.port,
        cv.Required(CONF_MOUNT_PATH): validate_mount_path,
        cv.Optional(CONF_UID, default=DEFAULT_UID): cv.int_range(min=0, max=65535),
        cv.Optional(CONF_GID, default=DEFAULT_GID): cv.int_range(min=0, max=65535),
        # Fire one mount attempt on each rising edge of network connectivity (wifi,
        # ethernet, modem or openthread -- whichever the config uses). No periodic retry:
        # schedule retries yourself via interval:/automations calling storage.mount.
        cv.Optional(CONF_AUTO_CONNECT, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.All(
    cv.ensure_list(NFS_SHARE_SCHEMA),
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
        cg.add(var.set_auto_connect(share_config[CONF_AUTO_CONNECT]))

        cg.add(var.set_mount_path(share_config[CONF_MOUNT_PATH]))
        # Full VFS paths carry the mount point; the storage component sizes its buffers from
        # the paths registered here.
        register_mount_path(share_config[CONF_MOUNT_PATH])

        request_storage_device()
        # NFSv3 NFS_MAXNAMLEN is 255; plus the terminator.
        request_path_length(256)
        request_storage_worker(task_safe=True)
