from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # to_code must run: it defines USE_NOISE and adds the noise-c library
    # the api benchmark sources need.
    manifest.enable_codegen()
