"""Build the BSEC2 component on the host against the BSEC2/BME68x double in __sdk."""

from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

# bsec_types.h, shared with the BSEC 1.x double, is read from the source tree, so it is found even when the
# bme680_bsec tests are not part of the build.
_SHARED_SDK_DIR = Path(__file__).resolve().parent.parent / "bme680_bsec" / "__sdk"


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = ["sensor", "text_sensor"]
    manifest.config_schema = cv.Schema({})

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_build_flag(f"-I{CORE.relative_src_path('bme68x_bsec2', '__sdk')}")
        cg.add_build_flag(f"-I{_SHARED_SDK_DIR}")
        cg.add_define("USE_BSEC2")
        cg.add_define("USE_SENSOR")
        cg.add_define("ESPHOME_ENTITY_SENSOR_COUNT", 0)
        cg.add_define("USE_TEXT_SENSOR")
        cg.add_define("ESPHOME_ENTITY_TEXT_SENSOR_COUNT", 0)

    manifest.to_code = to_code_testing
