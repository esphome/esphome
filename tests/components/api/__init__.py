import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # USE_API compiles every api source, so emit what they need. No socket
    # override: an __init__.py there makes pytest import its conftest as socket.conftest.
    async def to_code_testing(config):
        cg.add_define("USE_API")
        cg.add_define("USE_API_PLAINTEXT")
        # Linked wizard inputs only exist next to homeassistant entities, which need this
        cg.add_define("USE_API_HOMEASSISTANT_STATES")
        # test_wizard.cpp supplies the tables that codegen emits for a real build
        for define in (
            "USE_API_WIZARD",
            "USE_API_WIZARD_INPUTS",
            "USE_API_WIZARD_LINKED_INPUTS",
            "USE_API_WIZARD_STANDALONE_INPUTS",
        ):
            cg.add_define(define)
        cg.add_define("API_WIZARD_DATA_SIZE", 200)
        cg.add_define("API_WIZARD_INPUT_COUNT", 2)
        cg.add_define("API_MAX_SEND_QUEUE", 8)
        cg.add_define("MAX_API_CONNECTIONS", 1)
        cg.add_define("USE_SOCKET_IMPL_BSD_SOCKETS")

    manifest.to_code = to_code_testing
