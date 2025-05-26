#pragma once

#include <NimBLEDevice.h>
#include <Sesame.h>
#include <libsesame3bt/BLEBackend.h>
#include <libsesame3bt/ServerCore.h>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace libsesame3bt {

using registration_callback_t =
    std::function<void(const NimBLEAddress& addr, const std::array<std::byte, Sesame::SECRET_SIZE>& secret)>;
using command_callback_t =
    std::function<Sesame::result_code_t(const NimBLEAddress& addr, Sesame::item_code_t cmd, const std::string& tag)>;

class SesameServer : private NimBLEServerCallbacks, private NimBLECharacteristicCallbacks, private core::ServerBLEBackend {
 public:
	SesameServer(size_t max_sessions) : core(*this, max_sessions) {}
	SesameServer(const SesameServer&) = delete;
	virtual ~SesameServer() {}

	bool begin(Sesame::model_t model, const NimBLEAddress& server_address, const NimBLEUUID& uuid);
	bool start_advertising();
	bool stop_advertising();
	void update();
	bool set_registered(const std::array<std::byte, Sesame::SECRET_SIZE>& secret) { return core.set_registered(secret); }
	void set_on_registration_callback(registration_callback_t callback) { registration_callback = callback; }
	void set_on_command_callback(command_callback_t callback) { command_callback = callback; }
	size_t get_session_count() { return core.get_session_count(); }
	bool is_registered() const { return core.is_registered(); }
	bool send_lock_status(bool locked);
	bool has_session(const NimBLEAddress& addr) const;
	void disconnect(const NimBLEAddress& addr);

 private:
	NimBLEAdvertising* adv = nullptr;
	NimBLEServer* ble_server = nullptr;
	NimBLEService* srv = nullptr;
	NimBLECharacteristic* tx = nullptr;
	NimBLECharacteristic* rx = nullptr;
	registration_callback_t registration_callback;
	command_callback_t command_callback;

	core::SesameServerCore core;

	virtual void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
	virtual void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
	virtual void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;
	virtual void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
	virtual void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
	virtual bool write_to_central(uint16_t session_id, const uint8_t* data, size_t size) override;
	virtual void disconnect(uint16_t session_id) override;
	void on_registration(uint16_t session_id, const std::array<std::byte, Sesame::SECRET_SIZE>& secret);
	Sesame::result_code_t on_command(uint16_t session_id, Sesame::item_code_t cmd, const std::string& tag);
	bool send_notify(std::optional<uint16_t> session_id,
	                 Sesame::op_code_t op_code,
	                 Sesame::item_code_t item_code,
	                 const std::byte* data,
	                 size_t size) {
		return core.send_notify(session_id, op_code, item_code, data, size);
	}
	bool set_advertising_data();
};

}  // namespace libsesame3bt
