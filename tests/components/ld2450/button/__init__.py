import esphome.codegen as cg
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Turns on the xy_unfiltered button and the X/Y text sensors so the window code is in the host test build
    async def to_code_testing(config: ConfigType) -> None:
        CORE.testing_ensure_platform_registered("button")
        CORE.testing_ensure_platform_registered("text_sensor")
        cg.add_define("USE_LD2450_TARGET_XY")
        cg.add_define("USE_LD2450_XY_UNFILTERED")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["button", "text_sensor"]
