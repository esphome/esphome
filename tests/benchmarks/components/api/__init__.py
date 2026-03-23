from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # api must run its to_code to define USE_API, USE_API_PLAINTEXT,
    # and add the noise-c library dependency.
    manifest.enable_codegen()
