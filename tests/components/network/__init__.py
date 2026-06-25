import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.enable_codegen()
    real_to_code = manifest.to_code

    async def to_code_testing(config):
        await real_to_code(config)
        cg.add_define("USE_NETWORK_IPV6", True)

    manifest.to_code = to_code_testing
