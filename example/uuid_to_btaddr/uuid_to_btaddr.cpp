#include <NimBLEAddress.h>
#include <NimBLEUUID.h>
#include <SesameServer.h>
#if __has_include("mysesame-config.h")
#include "mysesame-config.h"
#endif
#include <libsesame3bt/util.h>

#ifndef SESAME_SERVER_UUID
#define SESAME_SERVER_UUID "***Replace this***"
#endif

using libsesame3bt::SesameServer;

void
setup() {
	Serial.begin(115200);
	delay(5000);

	NimBLEUUID uuid{SESAME_SERVER_UUID};
	auto addr = SesameServer::uuid_to_ble_address(uuid);
	if (addr.isNull()) {
		Serial.println("Failed to convert UUID to BLE address");
		return;
	}
	Serial.printf("uuid %s => addr %s\n", uuid.toString().c_str(), addr.toString().c_str());
}

void
loop() {
	delay(1000);
}
