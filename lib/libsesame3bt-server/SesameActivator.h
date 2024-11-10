#pragma once

#include <NimBLEDevice.h>
#include <Sesame.h>
#include <string>
#include <unordered_map>

namespace libsesame3bt {

class SesameActivator {
 public:
	SesameActivator();
	virtual ~SesameActivator() {}


	bool begin(const NimBLEUUID& uuid, libsesame3bt::Sesame::model_t model);
	bool start_advertising();
	bool stop_advertising();

 private:
	NimBLEAdvertising* adv = nullptr;
	NimBLEServer* server = nullptr;
	NimBLEService* srv = nullptr;
	NimBLECharacteristic* tx = nullptr;
	NimBLECharacteristic* rx = nullptr;
	std::unordered_map<std::string, ClientState> clients;

	virtual void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) override;
};

}  // namespace libsesame3bt
