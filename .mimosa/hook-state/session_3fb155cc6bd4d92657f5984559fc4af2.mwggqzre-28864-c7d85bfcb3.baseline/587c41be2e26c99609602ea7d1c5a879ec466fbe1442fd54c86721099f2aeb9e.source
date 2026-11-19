from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # logger must run its to_code during builds because it configures
    # the logging subsystem used by ESP_LOG* macros.
    manifest.enable_codegen()
