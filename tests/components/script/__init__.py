import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config):
        cg.add_define("USE_SCRIPT_ON_STOP")

    manifest.to_code = to_code_testing
