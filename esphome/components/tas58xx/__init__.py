from esphome.config_helpers import filter_source_files_from_defines

CODEOWNERS = ["@mrtoy-me", "@remcom"]
DOMAIN = "tas58xx"

FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {
        "model_tas5805m.cpp": "USE_TAS58XX_TAS5805M",
        "model_tas5825m.cpp": "USE_TAS58XX_TAS5825M",
    }
)
