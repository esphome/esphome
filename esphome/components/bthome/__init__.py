from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv

CODEOWNERS = ["@Bascht74"]

# No hub keys. `bthome:` only links the codec so the host test builds.
# The button platform lives in binary_sensor.py.
CONFIG_SCHEMA = cv.Schema({})

# binary_sensor.cpp needs the binary_sensor component. The host test loads
# `bthome:` alone, so keep that file out until the platform is configured.
FILTER_SOURCE_FILES = filter_source_files_from_defines({"binary_sensor.cpp": "USE_BTHOME_BINARY_SENSOR"})
