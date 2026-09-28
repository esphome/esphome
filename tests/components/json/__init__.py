from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # json's to_code calls cg.add_library("bblanchon/ArduinoJson", ...). C++
    # unit test builds that pull json in transitively (e.g. api) need that
    # library registration to happen, otherwise json_util.cpp fails to find
    # ArduinoJson.h.
    manifest.enable_codegen()
