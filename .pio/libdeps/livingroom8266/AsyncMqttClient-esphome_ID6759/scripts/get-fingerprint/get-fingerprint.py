#!/usr/bin/env python

import argparse
import ssl
import hashlib

parser = argparse.ArgumentParser(description='Compute SSL/TLS fingerprints.')
parser.add_argument('--host', required=True)
parser.add_argument('--port', default=8883)

args = parser.parse_args()
print(args.host)

cert_pem = ssl.get_server_certificate((args.host, args.port))
cert_der = ssl.PEM_cert_to_DER_cert(cert_pem)

md5 = hashlib.md5(cert_der).hexdigest()
sha1 = hashlib.sha1(cert_der).hexdigest()
sha256 = hashlib.sha256(cert_der).hexdigest()
print("MD5: " + md5)
print("SHA1: " + sha1)
print("SHA256: " + sha256)

print("\nSHA1 as array initializer:")
print("const uint8_t fingerprint[] = {0x" + ", 0x".join([sha1[i:i+2] for i in range(0, len(sha1), 2)]) + "};")

print("\nSHA1 as function call:")
print("mqttClient.addServerFingerprint((const uint8_t[]){0x" + ", 0x".join([sha1[i:i+2] for i in range(0, len(sha1), 2)]) + "});")
