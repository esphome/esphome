from pathlib import Path

import esphome.codegen as cg
from esphome.const import CONF_ESPHOME, CONF_INCLUDES
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

TESTS_DIR = Path(__file__).parent


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # USE_WIFI compiles wifi_component.cpp, which links against the platform hooks that
    # host_hooks.cpp stubs; that file only builds with wifi's own tests
    async def to_code_testing(config: ConfigType) -> None:
        if any(Path(p) == TESTS_DIR for p in CORE.config[CONF_ESPHOME][CONF_INCLUDES]):
            cg.add_define("USE_WIFI")

    manifest.to_code = to_code_testing
