import esphome.codegen as cg
from esphome.components import storage_host
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_USERNAME

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["storage_host", "web_server_base"]
AUTO_LOAD = []

http_file_server_ns = cg.esphome_ns.namespace("http_file_server")
HttpFileServer = http_file_server_ns.class_("HttpFileServer", cg.Component)

CONF_ROOT_PATH = "root_path"
CONF_URL_PREFIX = "url_prefix"
CONF_ENABLE_AUTH = "enable_auth"
CONF_STORAGE_HOST_ID = "storage_host_id"
CONF_ENABLE_UPLOAD = "enable_upload"
CONF_ENABLE_DOWNLOAD = "enable_download"
CONF_ENABLE_DELETION = "enable_deletion"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HttpFileServer),
        cv.Required(CONF_STORAGE_HOST_ID): cv.use_id(
            storage_host.StorageHost
        ),  # Reference to storage_host (REQUIRED)
        cv.Optional(CONF_ROOT_PATH, default="/"): cv.string,
        cv.Optional(CONF_URL_PREFIX, default="/files"): cv.string,
        cv.Optional(CONF_PORT, default=80): cv.port,
        cv.Optional(CONF_ENABLE_AUTH, default=False): cv.boolean,
        cv.Optional(CONF_USERNAME): cv.string,
        cv.Optional(CONF_PASSWORD): cv.string,
        cv.Optional(CONF_ENABLE_UPLOAD, default=False): cv.boolean,
        cv.Optional(CONF_ENABLE_DOWNLOAD, default=True): cv.boolean,
        cv.Optional(CONF_ENABLE_DELETION, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_HTTP_FILE_SERVER")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    storage_host_var = await cg.get_variable(config[CONF_STORAGE_HOST_ID])
    cg.add(var.set_storage_host(storage_host_var))

    root_path = config[CONF_ROOT_PATH]
    url_prefix = config[CONF_URL_PREFIX]
    port = config[CONF_PORT]
    enable_auth = config[CONF_ENABLE_AUTH]
    enable_upload = config[CONF_ENABLE_UPLOAD]
    enable_download = config[CONF_ENABLE_DOWNLOAD]
    enable_deletion = config[CONF_ENABLE_DELETION]

    cg.add(var.set_root_path(root_path))
    cg.add(var.set_url_prefix(url_prefix))
    cg.add(var.set_port(port))
    cg.add(var.set_upload_enabled(enable_upload))
    cg.add(var.set_download_enabled(enable_download))
    cg.add(var.set_deletion_enabled(enable_deletion))

    if enable_auth:
        if CONF_USERNAME in config and CONF_PASSWORD in config:
            username = config[CONF_USERNAME]
            password = config[CONF_PASSWORD]
            cg.add(var.set_credentials(username, password))
        else:
            raise cv.Invalid(
                "Username and password required when enabling authentication"
            )
