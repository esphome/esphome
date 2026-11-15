"""Build the BSEC component on the host against the BSEC/BME680 double in __sdk."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.helpers import write_file_if_changed
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

# set_config_() includes one of these library blobs; each gets distinct bytes so test_config.cpp can tell
# which one reached bsec_set_configuration().
_CONFIG_BLOBS = (
    "generic_33v_300s_28d",
    "generic_18v_300s_28d",
    "generic_33v_3s_28d",
    "generic_18v_3s_28d",
)


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = ["i2c", "sensor", "text_sensor"]
    manifest.multi_conf = False
    manifest.config_schema = cv.Schema({})

    async def to_code_testing(config: ConfigType) -> None:
        blobs = CORE.relative_build_path("bsec_config")
        for index, name in enumerate(_CONFIG_BLOBS):
            first = 0x10 * (index + 1)
            write_file_if_changed(
                blobs / "config" / name / "bsec_iaq.txt",
                f"{first}, {first + 1}, {first + 2}\n",
            )
        cg.add_build_flag(f"-I{blobs}")
        cg.add_build_flag(f"-I{CORE.relative_src_path('bme680_bsec', '__sdk')}")
        cg.add_define("USE_BSEC")
        cg.add_define("USE_SENSOR")
        cg.add_define("ESPHOME_ENTITY_SENSOR_COUNT", 0)
        cg.add_define("USE_TEXT_SENSOR")
        cg.add_define("ESPHOME_ENTITY_TEXT_SENSOR_COUNT", 0)

    manifest.to_code = to_code_testing
