import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The production codegen sizes this registry from configured sensor hubs.
    # The host unit test constructs two hubs directly instead.
    manifest.enable_codegen()

    async def to_code_testing(config):
        cg.add_define("VL6180X_INSTANCE_COUNT", 2)

    manifest.to_code = to_code_testing
