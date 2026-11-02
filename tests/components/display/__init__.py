from pathlib import Path

import esphome.codegen as cg
from esphome.const import CONF_ESPHOME, CONF_INCLUDES
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

TESTS_DIR = Path(__file__).parent


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The column walk in Display::draw_pixels_at() is only built with PSRAM; define it
    # when display's own tests are built so both walk orders run, and leave other
    # components' test binaries on the plain build.
    async def to_code_testing(config: ConfigType) -> None:
        if any(Path(p) == TESTS_DIR for p in CORE.config[CONF_ESPHOME][CONF_INCLUDES]):
            cg.add_define("USE_PSRAM")

    manifest.to_code = to_code_testing
