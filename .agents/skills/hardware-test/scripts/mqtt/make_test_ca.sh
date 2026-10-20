#!/bin/sh
# make_test_ca.sh BROKER_IP : a throwaway CA and a server certificate for the broker address (30 days), written next
# to this script where mosquitto-ws.conf expects them. The device
# config takes ca.crt as certificate_authority (skip_cert_cn_check: true, the name check does not match an address);
# other-ca.crt is a second CA for the negative case.
set -e
cd "$(dirname "$0")"
IP=$1
openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.crt -days 30 -subj "/CN=hwtest-ca"
openssl req -newkey rsa:2048 -nodes -keyout server.key -out server.csr -subj "/CN=hwtest-broker"
printf "subjectAltName=IP:%s\nextendedKeyUsage=serverAuth\n" "$IP" > san.ext
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 30 -extfile san.ext
openssl req -x509 -newkey rsa:2048 -nodes -keyout other-ca.key -out other-ca.crt -days 30 -subj "/CN=other-ca"
openssl verify -CAfile ca.crt server.crt
