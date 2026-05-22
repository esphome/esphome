from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # json must run its to_code during benchmark builds because it
    # adds the ArduinoJson library dependency needed by the API component.
    manifest.enable_codegen()
