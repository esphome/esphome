from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # socket must run its to_code to define USE_SOCKET_IMPL_BSD_SOCKETS for
    # the api overflow buffer tests.
    manifest.enable_codegen()
