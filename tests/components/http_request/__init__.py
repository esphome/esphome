import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The send action tests read the response body, which the real to_code enables for
    # configs that capture it.
    async def to_code_testing(config):
        cg.add_define("USE_HTTP_REQUEST_RESPONSE")

    manifest.to_code = to_code_testing
