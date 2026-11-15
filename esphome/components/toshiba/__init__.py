from esphome.config_helpers import filter_source_files_from_defines

DOMAIN = "toshiba"

FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {"ras_25j2kvsg_nd.cpp": "USE_TOSHIBA_RAS_25J2KVSG_ND"}
)
