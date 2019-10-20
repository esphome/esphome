#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <DNSServer.h>
#include <vector>

#include "config.h"

static DNSServer DNS;

static std::vector<AsyncClient*> clients; // a list to hold all clients

 /* clients events */
static void handleError(void* arg, AsyncClient* client, int8_t error) {
	Serial.printf("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}

static void handleData(void* arg, AsyncClient* client, void *data, size_t len) {
	Serial.printf("\n data received from client %s \n", client->remoteIP().toString().c_str());
	Serial.write((uint8_t*)data, len);

	// reply to client
	if (client->space() > 32 && client->canSend()) {
		char reply[32];
		sprintf(reply, "this is from %s", SERVER_HOST_NAME);
		client->add(reply, strlen(reply));
		client->send();
	}
}

static void handleDisconnect(void* arg, AsyncClient* client) {
	Serial.printf("\n client %s disconnected \n", client->remoteIP().toString().c_str());
}

static void handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
	Serial.printf("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}


/* server events */
static void handleNewClient(void* arg, AsyncClient* client) {
	Serial.printf("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());

	// add to list
	clients.push_back(client);
	
	// register events
	client->onData(&handleData, NULL);
	client->onError(&handleError, NULL);
	client->onDisconnect(&handleDisconnect, NULL);
	client->onTimeout(&handleTimeOut, NULL);
}

void setup() {
	Serial.begin(115200);
	delay(20);
	
	// create access point
	while (!WiFi.softAP(SSID, PASSWORD, 6, false, 15)) {
		delay(500);
	}

	// start dns server
	if (!DNS.start(DNS_PORT, SERVER_HOST_NAME, WiFi.softAPIP()))
		Serial.printf("\n failed to start dns service \n");

	AsyncServer* server = new AsyncServer(TCP_PORT); // start listening on tcp port 7050
	server->onClient(&handleNewClient, server);
	server->begin();
}

void loop() {
	DNS.processNextRequest();
}
