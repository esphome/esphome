from esphome.config_helpers import filter_source_files_from_defines

CODEOWNERS = ["@Bascht74"]

# binary_sensor.cpp needs the binary_sensor component. The host test loads
# this package alone, so keep that file out until the platform is configured.
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"binary_sensor.cpp": "USE_BTHOME_BINARY_SENSOR"}
)
