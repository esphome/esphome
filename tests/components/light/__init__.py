import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Enables light_json_schema.cpp without USE_MQTT, which pulls mqtt code into core/util.cpp
    async def to_code_testing(config):
        cg.add_define("USE_WEBSERVER")
        # api_connection.cpp reports the port whenever USE_WEBSERVER is set
        cg.add_define("USE_WEBSERVER_PORT", 80)
        # The gamma table and its stored gamma are behind this define
        cg.add_define("USE_LIGHT_GAMMA_LUT")

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["json"]
