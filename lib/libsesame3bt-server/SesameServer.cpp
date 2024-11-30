#include "SesameServer.h"
#include <libsesame3bt/ScannerCore.h>
#include <libsesame3bt/util.h>
#include "debug.h"

namespace libsesame3bt {

bool
SesameServer::begin(Sesame::model_t model, const NimBLEAddress& server_address, const NimBLEUUID& my_uuid) {
	core.set_on_registration_callback([this](auto session_id, const auto& secret) { on_registration(session_id, secret); });
	core.set_on_command_callback(
	    [this](uint16_t session_id, Sesame::item_code_t cmd, const std::string& tag) { return on_command(session_id, cmd, tag); });

	if (!NimBLEDevice::init("Peripheral Demo") || !NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM) ||
	    !NimBLEDevice::setOwnAddr(server_address)) {
		Serial.println("Failed to init BLE");
		return false;
	}

	auto r_uuid = my_uuid;
	r_uuid.to128();
	r_uuid.reverseByteOrder();
	if (!core.begin(model, *reinterpret_cast<const uint8_t(*)[16]>(r_uuid.getValue())) || !core.generate_keypair()) {
		return false;
	}

	adv = NimBLEDevice::getAdvertising();
	adv->setMinInterval(1000);
	adv->setMaxInterval(1500);
	adv->addServiceUUID(NimBLEUUID{Sesame::SESAME3_SRV_UUID});
	auto [manu, name] = core.create_advertisement_data_os3();
	adv->setManufacturerData(manu);
	adv->setName(name);

	ble_server = NimBLEDevice::createServer();
	ble_server->setCallbacks(this, false);
	srv = ble_server->createService(NimBLEUUID{Sesame::SESAME3_SRV_UUID});
	rx = srv->createCharacteristic(NimBLEUUID{Sesame::TxUUID}, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
	rx->setCallbacks(this);
	tx = srv->createCharacteristic(NimBLEUUID(Sesame::RxUUID), NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
	tx->setCallbacks(this);
	srv->start();
	// create dummy service to limit end handle value of CANDY HOUSE service group (SESAME3_SRV_UUID(0xfd81))
	auto srv2 = ble_server->createService(NimBLEUUID(static_cast<uint32_t>(0xfefefefe)));
	srv2->start();
	ble_server->start();

	return true;
}

void
SesameServer::update() {
	if (!ble_server) {
		return;
	}
	core.update();
}

bool
SesameServer::send_lock_status(bool locked) {
	Sesame::mecha_status_5_t status{};
	status.battery = 10;
	status.in_lock = locked;
	status.is_stop = true;
	return send_notify({}, Sesame::op_code_t::publish, Sesame::item_code_t::mech_status, reinterpret_cast<std::byte*>(&status),
	                   sizeof(status));
}

void
SesameServer::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
	DEBUG_PRINTLN("Connected from = %s", connInfo.getAddress().toString().c_str());
	start_advertising();
}

void
SesameServer::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
	DEBUG_PRINTLN("Disconnected from = %s", connInfo.getAddress().toString().c_str());
	core.on_disconnected(connInfo.getConnHandle());
}

bool
SesameServer::start_advertising() {
	if (adv) {
		return adv->start();
	}
	return false;
}

bool
SesameServer::stop_advertising() {
	if (adv) {
		return adv->stop();
	}
	return false;
}

void
SesameServer::onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) {
	if (pCharacteristic != tx) {
		DEBUG_PRINTLN("Unexpected endpoint subscribed");
		ble_server->disconnect(connInfo);
		return;
	}
	DEBUG_PRINTLN("Subscribed from=%s, val=%u", connInfo.getAddress().toString().c_str(), subValue);
	if ((subValue & 1)) {
		if (!core.on_subscribed(connInfo.getConnHandle())) {
			ble_server->disconnect(connInfo);
		}
	}
}

void
SesameServer::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
	if (pCharacteristic == rx) {
		DEBUG_PRINTLN("onRead RX(ignored)");
	} else if (pCharacteristic == tx) {
		DEBUG_PRINTLN("onRead TX(ignored)");
	}
}

void
SesameServer::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) {
	if (pCharacteristic == rx) {
		auto val = pCharacteristic->getValue();
		if (!core.on_received(connInfo.getConnHandle(), reinterpret_cast<const std::byte*>(val.data()), val.size())) {
			DEBUG_PRINTLN("core.on_received failed, disconnect");
			ble_server->disconnect(connInfo);
		}
	} else if (pCharacteristic == tx) {
		DEBUG_PRINTLN("onWrite TX(ignored)");
	}
}

bool
SesameServer::write_to_central(uint16_t session_id, const uint8_t* data, size_t size) {
	if (tx) {
		DEBUG_PRINTLN("TX characterristic not created, cannot proceed");
		tx->notify(data, size, session_id);
		return true;
	}
	return false;
}

void
SesameServer::disconnect(uint16_t session_id) {
	if (ble_server->disconnect(session_id) != 0) {
		DEBUG_PRINTLN("Failed to disconnect session %u", session_id);
	}
}

void
SesameServer::on_registration(uint16_t session_id, const std::array<std::byte, Sesame::SECRET_SIZE>& secret) {
	registered = true;
	adv->stop();
	auto [manu, name] = core.create_advertisement_data_os3();
	adv->setManufacturerData(manu);
	adv->setName(name);
	adv->start();
	if (registration_callback) {
		registration_callback(ble_server->getPeerIDInfo(session_id).getAddress(), secret);
	}
}

Sesame::result_code_t
SesameServer::on_command(uint16_t session_id, Sesame::item_code_t cmd, const std::string& tag) {
	if (command_callback) {
		return command_callback(ble_server->getPeerIDInfo(session_id).getAddress(), cmd, tag);
	} else {
		return Sesame::result_code_t::not_supported;
	}
}

}  // namespace libsesame3bt
