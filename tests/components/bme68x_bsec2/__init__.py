"""Build the BSEC2 component on the host against the BSEC2/BME68x double in __sdk."""

from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.helpers import write_file_if_changed
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

# bsec_types.h is shared with the BSEC 1.x double. Only that header is copied into its own build directory, so it
# is found without the bme680_bsec tests and the BSEC 1.x bsec.h never lands on the include path twice.
_SHARED_TYPES = (
    Path(__file__).resolve().parent.parent / "bme680_bsec" / "__sdk" / "bsec_types.h"
)


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = ["sensor", "text_sensor"]
    manifest.config_schema = cv.Schema({})

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_build_flag(f"-I{CORE.relative_src_path('bme68x_bsec2', '__sdk')}")
        shared = CORE.relative_build_path("bsec_shared")
        write_file_if_changed(shared / "bsec_types.h", _SHARED_TYPES.read_text())
        cg.add_build_flag(f"-I{shared}")
        cg.add_define("USE_BSEC2")
        cg.add_define("USE_SENSOR")
        cg.add_define("ESPHOME_ENTITY_SENSOR_COUNT", 0)
        cg.add_define("USE_TEXT_SENSOR")
        cg.add_define("ESPHOME_ENTITY_TEXT_SENSOR_COUNT", 0)

    manifest.to_code = to_code_testing
