from esphome.components.esp32 import (
    add_idf_sdkconfig_option,
    include_builtin_idf_component,
)
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@dentra"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_on_esp32,
)


async def to_code(config: ConfigType) -> None:
    # Increase the maximum supported size of headers section in HTTP request packet to be processed by the server
    add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 1024)
    # Re-enable esp-tls (excluded by default to save compile time);
    # web_server_idf.cpp includes <esp_tls_crypto.h> for digest auth
    include_builtin_idf_component("esp-tls")
    include_builtin_idf_component("esp_http_server")


# multipart.cpp is fully #ifdef'd on USE_WEBSERVER_OTA (set by the
# web_server OTA platform); skip it when OTA uploads are not configured.
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"multipart.cpp": "USE_WEBSERVER_OTA"}
)
