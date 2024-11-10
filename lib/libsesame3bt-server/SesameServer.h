#pragma once

#include <NimBLEDevice.h>
#include <Sesame.h>
#include <libsesame3bt/BLEBackend.h>
#include <libsesame3bt/ServerCore.h>
#include <functional>
#include <string>
#include <unordered_map>

namespace libsesame3bt {

using registration_callback_t = std::function<void(NimBLEAddress, const std::array<std::byte, Sesame::SECRET_SIZE>& secret)>;
using command_callback_t = std::function<Sesame::result_code_t(NimBLEAddress, Sesame::item_code_t cmd, const std::string& tag)>;

class SesameServer : private NimBLEServerCallbacks, private NimBLECharacteristicCallbacks, private core::ServerBLEBackend {
 public:
	SesameServer(size_t max_sessions) : core(*this, max_sessions) {}
	SesameServer(const SesameServer&) = delete;
	virtual ~SesameServer() {}

	bool begin(libsesame3bt::Sesame::model_t model, const NimBLEUUID& uuid);
	bool start_advertising();
	bool stop_advertising();
	void update();
	bool load_key(const std::array<std::byte, Sesame::SK_SIZE>& privkey) { return core.load_key(privkey); }
	bool export_keypair(std::array<std::byte, Sesame::PK_SIZE>& pubkey, std::array<std::byte, Sesame::SK_SIZE>& privkey) {
		return core.export_keypair(pubkey, privkey);
	}
	bool generate_keypair() { return core.generate_keypair(); }
	bool set_registered(const std::array<std::byte, Sesame::SECRET_SIZE>& secret) { return core.set_registered(secret); }
	void set_on_registration_callback(registration_callback_t callback) { registration_callback = callback; }
	void set_on_command_callback(command_callback_t callback) { command_callback = callback; }
	size_t get_session_count() { return core.get_session_count(); }

 private:
	NimBLEAdvertising* adv = nullptr;
	NimBLEServer* ble_server = nullptr;
	NimBLEService* srv = nullptr;
	NimBLECharacteristic* tx = nullptr;
	NimBLECharacteristic* rx = nullptr;
	registration_callback_t registration_callback;
	command_callback_t command_callback;
	bool registered;

	core::SesameServerCore core;

	virtual void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
	virtual void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
	virtual void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;
	virtual void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
	virtual void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
	virtual bool write_to_peripheral(uint16_t session_id, const uint8_t* data, size_t size) override;
	virtual void disconnect(uint16_t session_id) override;
	void on_registration(uint16_t session_id, const std::array<std::byte, Sesame::SECRET_SIZE>& secret);
	Sesame::result_code_t on_command(uint16_t session_id, Sesame::item_code_t cmd, const std::string& tag);
};

}  // namespace libsesame3bt
