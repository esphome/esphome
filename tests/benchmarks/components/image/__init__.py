import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Loops that only exist on boards with PSRAM are gated on this define;
    # set it so the host benchmark measures the same code those boards run.
    manifest.enable_codegen()
    original_to_code = manifest.to_code

    async def to_code(config: ConfigType) -> None:
        await original_to_code(config)
        cg.add_define("USE_PSRAM")

    if hasattr(original_to_code, "priority"):
        to_code.priority = original_to_code.priority
    manifest.to_code = to_code
