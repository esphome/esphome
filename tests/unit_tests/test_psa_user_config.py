"""The esp32 PSA user config header keeps each mbedTLS algorithm for exactly the
sdkconfig options that need it. The C preprocessor alone decides this, so each
condition is checked on its own with the host compiler."""

from __future__ import annotations

import shutil
import subprocess

import pytest

from esphome.components.esp32 import PSA_USER_CONFIG_SOURCE

_COMPILER = next(
    (cc for cc in ("cc", "gcc", "clang") if shutil.which(cc) is not None), None
)

# What TF-PSA-Crypto's crypto_config.h defines before esp_config.h includes
# the header.
_PSA_DEFAULTS = (
    "PSA_WANT_ALG_FFDH",
    "PSA_WANT_DH_RFC7919_2048",
    "PSA_WANT_KEY_TYPE_DH_PUBLIC_KEY",
    "PSA_WANT_ALG_RSA_PSS",
    "PSA_WANT_ALG_RSA_PSS_ANY_SALT",
)


def _defines(config: tuple[str, ...]) -> set[str]:
    source = "".join(f"#define {name} 1\n" for name in (*config, *_PSA_DEFAULTS))
    source += f'#include "{PSA_USER_CONFIG_SOURCE.as_posix()}"\n'
    out = subprocess.run(
        [_COMPILER, "-dM", "-E", "-x", "c", "-"],
        input=source,
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return {line.split()[1] for line in out.splitlines() if line.startswith("#define")}


@pytest.mark.skipif(_COMPILER is None, reason="needs a host C compiler")
@pytest.mark.parametrize(
    ("config", "ffdh", "rsa_pss"),
    [
        pytest.param((), False, False, id="nothing_needs_them"),
        pytest.param(("CONFIG_MBEDTLS_DHM_C",), True, False, id="dhe_keeps_ffdh"),
        pytest.param(
            ("CONFIG_MBEDTLS_SSL_PROTO_TLS1_3",), True, False, id="tls13_keeps_ffdh"
        ),
        pytest.param(("CONFIG_MBEDTLS_RSA_C",), False, True, id="rsa_keeps_pss"),
    ],
)
def test_psa_user_config_conditions(
    config: tuple[str, ...], ffdh: bool, rsa_pss: bool
) -> None:
    defines = _defines(config)
    assert ("PSA_WANT_ALG_FFDH" in defines) is ffdh
    assert ("PSA_WANT_DH_RFC7919_2048" in defines) is ffdh
    assert ("PSA_WANT_KEY_TYPE_DH_PUBLIC_KEY" in defines) is ffdh
    assert ("PSA_WANT_ALG_RSA_PSS" in defines) is rsa_pss
    assert ("PSA_WANT_ALG_RSA_PSS_ANY_SALT" in defines) is rsa_pss
