import esphome.config_validation as cv

CODEOWNERS = ["@Bascht74"]

# No keys yet. `bthome:` only pulls this translation unit into the build so the
# host test can link it. Receiver and transmitter config land in follow-up PRs.
CONFIG_SCHEMA = cv.Schema({})
