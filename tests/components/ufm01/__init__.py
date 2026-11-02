import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The optional features are define-gated; emit them so the tests cover those paths
    async def to_code_testing(config):
        cg.add_define("USE_UFM01_METER_ID")
        cg.add_define("USE_UFM01_SOFTWARE_VERSION")
        cg.add_define("USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION")

    manifest.to_code = to_code_testing
