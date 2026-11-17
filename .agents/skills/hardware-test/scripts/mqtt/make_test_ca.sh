#!/bin/sh
# make_test_ca.sh BROKER_IP : a throwaway CA, a server certificate for the broker address and a client certificate
# (30 days), written into the working directory; run it in the scratch directory where a copy of mosquitto-ws.conf
# lives. The device config takes ca.crt as certificate_authority (skip_cert_cn_check: true, the name check does not
# match an address) and client.crt/client.key as client_certificate/client_certificate_key for the listener that
# requires one; other-ca.crt is a second CA for the negative case. The CA carries basicConstraints and keyUsage
# because Python's default SSL context (esphome logs, esphome clean-mqtt) rejects a CA without them.
set -e
IP=$1
[ -n "$IP" ] || { echo "usage: make_test_ca.sh BROKER_IP"; exit 2; }
openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.crt -days 30 -subj "/CN=hwtest-ca" \
  -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign"
openssl req -newkey rsa:2048 -nodes -keyout server.key -out server.csr -subj "/CN=hwtest-broker"
printf "subjectAltName=IP:%s\nextendedKeyUsage=serverAuth\n" "$IP" > san.ext
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 30 -extfile san.ext
openssl req -newkey rsa:2048 -nodes -keyout client.key -out client.csr -subj "/CN=hwtest-client"
printf "extendedKeyUsage=clientAuth\n" > client.ext
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 30 -extfile client.ext
openssl req -x509 -newkey rsa:2048 -nodes -keyout other-ca.key -out other-ca.crt -days 30 -subj "/CN=other-ca" \
  -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign"
openssl verify -CAfile ca.crt server.crt client.crt
