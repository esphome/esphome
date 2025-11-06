import esphome.codegen as cg
from esphome.components import storage_host, web_server_base
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["storage_host", "web_server_base"]
AUTO_LOAD = []

http_file_server_ns = cg.esphome_ns.namespace("http_file_server")
HttpFileServer = http_file_server_ns.class_("HttpFileServer", cg.Component)

CONF_ROOT_PATH = "root_path"
CONF_URL_PREFIX = "url_prefix"
CONF_STORAGE_HOST_ID = "storage_host_id"
CONF_ENABLE_UPLOAD = "enable_upload"
CONF_ENABLE_DOWNLOAD = "enable_download"
CONF_ENABLE_DELETION = "enable_deletion"
CONF_AUTH_ENABLED = "auth_enabled"
CONF_USERNAME = "username"
CONF_PASSWORD = "password"


def validate_auth(config):
    """Validate authentication configuration."""
    auth_enabled = config.get(CONF_AUTH_ENABLED, False)
    has_username = CONF_USERNAME in config
    has_password = CONF_PASSWORD in config

    if auth_enabled:
        # If auth is enabled, username and password are required
        if not has_username or not has_password:
            raise cv.Invalid(
                f"When {CONF_AUTH_ENABLED} is true, both {CONF_USERNAME} and {CONF_PASSWORD} are required"
            )
    # If auth is disabled, username and password must not be set
    elif has_username or has_password:
        raise cv.Invalid(
            f"{CONF_USERNAME} and {CONF_PASSWORD} can only be set when {CONF_AUTH_ENABLED} is true"
        )

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HttpFileServer),
            cv.GenerateID(web_server_base.CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Required(CONF_STORAGE_HOST_ID): cv.use_id(
                storage_host.StorageHost
            ),  # Reference to storage_host (REQUIRED)
            cv.Optional(CONF_ROOT_PATH, default="/"): cv.string,
            cv.Optional(CONF_URL_PREFIX, default="/files"): cv.string,
            cv.Optional(CONF_ENABLE_UPLOAD, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_DOWNLOAD, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_DELETION, default=False): cv.boolean,
            cv.Optional(CONF_AUTH_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_USERNAME): cv.string,
            cv.Optional(CONF_PASSWORD): cv.string,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_auth,
)


@coroutine_with_priority(45.0)
async def to_code(config):
    cg.add_define("USE_HTTP_FILE_SERVER")

    # Get web_server_base instance
    web_server_base_var = await cg.get_variable(
        config[web_server_base.CONF_WEB_SERVER_BASE_ID]
    )

    # Create HttpFileServer with web_server_base as constructor parameter
    var = cg.new_Pvariable(config[CONF_ID], web_server_base_var)
    await cg.register_component(var, config)

    # Get storage_host instance
    storage_host_var = await cg.get_variable(config[CONF_STORAGE_HOST_ID])
    cg.add(var.set_storage_host(storage_host_var))

    # Set configuration
    root_path = config[CONF_ROOT_PATH]
    url_prefix = config[CONF_URL_PREFIX]
    enable_upload = config[CONF_ENABLE_UPLOAD]
    enable_download = config[CONF_ENABLE_DOWNLOAD]
    enable_deletion = config[CONF_ENABLE_DELETION]

    cg.add(var.set_root_path(root_path))
    cg.add(var.set_url_prefix(url_prefix))
    cg.add(var.set_upload_enabled(enable_upload))
    cg.add(var.set_download_enabled(enable_download))
    cg.add(var.set_deletion_enabled(enable_deletion))

    # Optional authentication
    if config.get(CONF_AUTH_ENABLED, False):
        cg.add(var.set_auth(config[CONF_USERNAME], config[CONF_PASSWORD]))

    # Register USB MSC devices for mount/unmount API
    from esphome.core import CORE

    if hasattr(CORE, "data") and "usb_msc_devices" in CORE.data:
        for usb_device in CORE.data["usb_msc_devices"]:
            cg.add(var.register_usb_msc_device(usb_device))

    # Register SD MMC devices for mount/unmount API
    if hasattr(CORE, "data") and "sd_mmc_devices" in CORE.data:
        for sd_device in CORE.data["sd_mmc_devices"]:
            cg.add(var.register_sd_mmc_device(sd_device))
