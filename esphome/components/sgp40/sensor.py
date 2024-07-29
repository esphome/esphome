import esphome.config_validation as cv

CODEOWNERS = ["@SenexCrenshaw"]

CONFIG_SCHEMA = cv.invalid(
    "SGP40 is deprecated.\nPlease use the SGP4x platform instead.\nSGP4x supports both SPG40 and SGP41.\n"
    " See https://esphome.io/components/sensor/sgp4x.html"
)
