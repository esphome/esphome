import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The real schema needs an ESP32 target and Wi-Fi; the host test build has neither.
    manifest.dependencies = ["binary_sensor", "sensor", "button", "select"]
    manifest.config_schema = cv.Schema({})
    manifest.final_validate_schema = None

    async def to_code_testing(config: ConfigType) -> None:
        # Resolve <espectre_sdk.h> to the test double copied next to the tests.
        cg.add_build_flag(f"-I{CORE.relative_src_path('espectre')}")
        cg.add_define("USE_ESPECTRE")
        # No entity platforms are configured, so count them to emit USE_BINARY_SENSOR,
        # USE_SENSOR and USE_SELECT for the entity code under test.
        CORE.platform_counts["binary_sensor"] += 2
        CORE.platform_counts["sensor"] += 1
        CORE.platform_counts["select"] += 1

    manifest.to_code = to_code_testing
