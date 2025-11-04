import esphome.codegen as cg
from esphome.components import storage_host
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_USERNAME

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["storage_host", "web_server_base"]
AUTO_LOAD = []

webdav_server_ns = cg.esphome_ns.namespace("webdav_server")
WebDAVServer = webdav_server_ns.class_("WebDAVServer", cg.Component)

CONF_ROOT_PATH = "root_path"
CONF_URL_PREFIX = "url_prefix"
CONF_ENABLE_AUTH = "enable_auth"
CONF_STORAGE_HOST_ID = "storage_host_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WebDAVServer),
        cv.GenerateID(CONF_STORAGE_HOST_ID): cv.use_id(
            storage_host.StorageHost
        ),  # Reference to storage_host
        cv.Optional(CONF_ROOT_PATH, default="/"): cv.string,
        cv.Optional(CONF_URL_PREFIX, default="/webdav"): cv.string,
        cv.Optional(CONF_PORT, default=8081): cv.port,
        cv.Optional(CONF_ENABLE_AUTH, default=False): cv.boolean,
        cv.Optional(CONF_USERNAME): cv.string,
        cv.Optional(CONF_PASSWORD): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_WEBDAV_SERVER")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    storage_host_var = await cg.get_variable(config[CONF_STORAGE_HOST_ID])
    cg.add(var.set_storage_host(storage_host_var))

    root_path = config[CONF_ROOT_PATH]
    url_prefix = config[CONF_URL_PREFIX]
    port = config[CONF_PORT]
    enable_auth = config[CONF_ENABLE_AUTH]

    cg.add(var.set_root_path(root_path))
    cg.add(var.set_url_prefix(url_prefix))
    cg.add(var.set_port(port))

    if enable_auth:
        if CONF_USERNAME in config and CONF_PASSWORD in config:
            username = config[CONF_USERNAME]
            password = config[CONF_PASSWORD]
            cg.add(var.set_credentials(username, password))
        else:
            raise cv.Invalid(
                "Username and password required when enabling authentication"
            )
