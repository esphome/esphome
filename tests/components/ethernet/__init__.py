from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, CONF_INCLUDES, CONF_USE_ADDRESS
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

TESTS_DIR = Path(__file__).parent


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.config_schema = cv.Schema(
        {cv.Optional(CONF_USE_ADDRESS, default="test.local"): cv.string_strict}
    )

    async def to_code_testing(config: ConfigType) -> None:
        # The host driver hooks only link when Ethernet's own tests are included.
        if any(Path(path) == TESTS_DIR for path in CORE.config[CONF_ESPHOME][CONF_INCLUDES]):
            cg.add_define("USE_ETHERNET")
            cg.add_define("USE_ETHERNET_CONNECT_TRIGGER")
            cg.add_define("USE_ETHERNET_DISCONNECT_TRIGGER")

    manifest.to_code = to_code_testing
