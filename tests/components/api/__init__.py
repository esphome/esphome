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
        # test_wizard.cpp supplies the API_WIZARD_PAGES table that codegen emits for a real build
        for define in (
            "USE_API_WIZARD",
            "USE_API_WIZARD_ENTITIES",
            "USE_API_WIZARD_ENTITY_FILTERS",
            "USE_API_WIZARD_INPUTS",
            "USE_API_WIZARD_LINKED_INPUTS",
            "USE_API_WIZARD_STANDALONE_INPUTS",
        ):
            cg.add_define(define)
        # The host reads PROGMEM like RAM, so the tests exercise the code ESP8266 uses to copy text out of it
        cg.add_define("API_WIZARD_FLASH_STRINGS")
        cg.add_define("API_WIZARD_INPUT_COUNT", 2)
        cg.add_define("API_WIZARD_PAGE_SCRATCH_SIZE", 32)
        cg.add_define("API_WIZARD_FIELD_SCRATCH_SIZE", 16)
        cg.add_define("API_WIZARD_FILTER_SCRATCH_SIZE", 8)
        cg.add_define("API_WIZARD_LIST_SCRATCH_SIZE", 64)
        cg.add_define("API_MAX_SEND_QUEUE", 8)
        cg.add_define("MAX_API_CONNECTIONS", 1)
        cg.add_define("USE_SOCKET_IMPL_BSD_SOCKETS")

    manifest.to_code = to_code_testing
