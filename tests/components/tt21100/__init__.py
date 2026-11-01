import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # Button storage is sized by code generation; the gtests put up to 3 buttons on one hub
        cg.add_define("TT21100_BUTTON_COUNT", 3)

    manifest.to_code = to_code_testing
    # touchscreen.h includes display.h
    manifest.dependencies = manifest.dependencies + ["display"]
