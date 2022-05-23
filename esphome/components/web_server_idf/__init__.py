import esphome.config_validation as cv

CODEOWNERS = ["@dentra"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_with_esp_idf,
)
