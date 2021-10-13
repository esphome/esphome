"""Module for all WPA2 Utilities.

The cryptography package is loaded lazily in the functions
so that it doesn't crash if it's not installed.
"""
import logging
from pathlib import Path

from esphome.core import CORE
import esphome.config_validation as cv
from esphome.const import (
    CONF_USERNAME,
    CONF_IDENTITY,
    CONF_PASSWORD,
    CONF_CERTIFICATE,
    CONF_KEY,
)


_LOGGER = logging.getLogger(__name__)


def validate_cryptography_installed():
    try:
        import cryptography
    except ImportError as err:
        raise cv.Invalid(
            "This settings requires the cryptography python package. "
            "Please install it with `pip install cryptography`"
        ) from err

    if cryptography.__version__[0] < "2":
        raise cv.Invalid(
            "Please update your python cryptography installation to least 2.x "
            "(pip install -U cryptography)"
        )


def wrapped_load_pem_x509_certificate(value):
    validate_cryptography_installed()

    from cryptography import x509
    from cryptography.hazmat.backends import default_backend

    return x509.load_pem_x509_certificate(value.encode("UTF-8"), default_backend())


def wrapped_load_pem_private_key(value, password):
    validate_cryptography_installed()

    from cryptography.hazmat.primitives.serialization import load_pem_private_key
    from cryptography.hazmat.backends import default_backend

    if password:
        password = password.encode("UTF-8")
    return load_pem_private_key(value.encode("UTF-8"), password, default_backend())


def read_relative_config_path(value):
    # pylint: disable=unspecified-encoding
    return Path(CORE.relative_config_path(value)).read_text()


def _validate_load_certificate(value):
    value = cv.file_(value)
    try:
        contents = read_relative_config_path(value)
        return wrapped_load_pem_x509_certificate(contents)
    except ValueError as err:
        raise cv.Invalid(f"Invalid certificate: {err}")


def validate_certificate(value):
    _validate_load_certificate(value)
    # Validation result should be the path, not the loaded certificate
    return value


def _validate_load_private_key(key, cert_pw):
    key = cv.file_(key)
    try:
        contents = read_relative_config_path(key)
        return wrapped_load_pem_private_key(contents, cert_pw)
    except ValueError as e:
        raise cv.Invalid(
            f"There was an error with the EAP 'password:' provided for 'key' {e}"
        )
    except TypeError as e:
        raise cv.Invalid(f"There was an error with the EAP 'key:' provided: {e}")


def _check_private_key_cert_match(key, cert):
    from cryptography.hazmat.primitives.asymmetric import rsa, ec

    def check_match_a():
        return key.public_key().public_numbers() == cert.public_key().public_numbers()

    def check_match_b():
        return key.public_key() == cert

    private_key_types = {
        rsa.RSAPrivateKey: check_match_a,
        ec.EllipticCurvePrivateKey: check_match_a,
    }

    try:
        # pylint: disable=no-name-in-module
        from cryptography.hazmat.primitives.asymmetric import ed448, ed25519

        private_key_types.update(
            {
                ed448.Ed448PrivateKey: check_match_b,
                ed25519.Ed25519PrivateKey: check_match_b,
            }
        )
    except ImportError:
        # ed448, ed25519 not supported
        pass

    key_type = next((kt for kt in private_key_types if isinstance(key, kt)), None)
    if key_type is None:
        _LOGGER.warning(
            "Unrecognised EAP 'certificate:' 'key:' pair format: %s. Proceed with caution!",
            type(key),
        )
    elif not private_key_types[key_type]():
        raise cv.Invalid("The provided EAP 'key' is not valid for the 'certificate'.")


def validate_eap(value):
    if CONF_USERNAME in value:
        if CONF_IDENTITY not in value:
            _LOGGER.info("EAP 'identity:' is not set, assuming username.")
            value = value.copy()
            value[CONF_IDENTITY] = value[CONF_USERNAME]
        if CONF_PASSWORD not in value:
            raise cv.Invalid(
                "You cannot use the EAP 'username:' option without a 'password:'. "
                "Please provide the 'password:' key"
            )

    if CONF_CERTIFICATE in value or CONF_KEY in value:
        # Check the key is valid and for this certificate, just to check the user hasn't pasted
        # the wrong thing. I write this after I spent a while debugging that exact issue.
        # This may require a password to decrypt to key, so we should verify that at the same time.
        cert_pw = value.get(CONF_PASSWORD)
        key = _validate_load_private_key(value[CONF_KEY], cert_pw)

        cert = _validate_load_certificate(value[CONF_CERTIFICATE])
        _check_private_key_cert_match(key, cert)

    return value
