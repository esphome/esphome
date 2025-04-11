from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv

CODEOWNERS = ["@dentra"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_with_esp_idf,
)

AUTO_LOAD = ["web_server"]


async def to_code(config):
    # Increase the maximum supported size of headers section in HTTP request packet to be processed by the server
    add_idf_sdkconfig_option("CONFIG_HTTPD_MAX_REQ_HDR_LEN", 1024)
