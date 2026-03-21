from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # api must run its to_code during benchmark builds because it
    # defines USE_API, USE_API_PLAINTEXT, and USE_API_NOISE which
    # are needed by the frame helper headers.
    manifest.enable_codegen()
