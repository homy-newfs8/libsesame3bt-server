#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Sesame.h>
#include <SesameServer.h>
#include <TaskManagerIO.h>
#include <libsesame3bt/util.h>
#if __has_include("mysesame-config.h")
#include "mysesame-config.h"
#endif

/*
 * UUIDとBLE Addressは何らかの法則に従った値の組み合わせが必要と思われます。
 * 適当に生成した値を使うと Remote 等のデバイスが接続してこなくなります。
 */
/* SESAME DEVICE UUID in format 12345678-90ab-cdef-1234-567890abcdef */
#ifndef SESAME_SERVER_UUID
#define SESAME_SERVER_UUID "***Replace this***"
#endif

/* BLE address in format 01:02:03:04:05:06 */
#ifndef SESAME_SERVER_ADDRESS
#define SESAME_SERVER_ADDRESS "***Replace this***"
#endif

/*
 * 最大セッション数を設定
 * 3を超えるデバイスを扱う場合NimBLE-Arduinoの最大接続数も調整する必要があります。
 * コンパイルオプションでシンボルCONFIG_BT_NIMBLE_MAX_CONNECTIONSに9以下の数値を設定してください。
 */
#ifndef SESAME_SERVER_MAX_SESSIONS
#ifdef CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#define SESAME_SERVER_MAX_SESSIONS CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#else
#define SESAME_SERVER_MAX_SESSIONS 3
#endif
#else
#define SESAME_SERVER_MAX_SESSIONS 3
#endif

using libsesame3bt::Sesame;
using libsesame3bt::SesameServer;
namespace util = libsesame3bt::core::util;

NimBLEUUID my_uuid{SESAME_SERVER_UUID};
NimBLEAddress my_addr{SESAME_SERVER_ADDRESS, BLE_ADDR_RANDOM};

SesameServer server{SESAME_SERVER_MAX_SESSIONS};

bool initialized;

namespace {

constexpr size_t UUID_SIZE = 16;

// このサンプルではNVS領域にPreferencesを使って秘密情報を保存しています
constexpr const char prefs_name[] = "sesameserver";
constexpr const char prefs_uuid[] = "uuid";
constexpr const char prefs_secret[] = "secret";
// 起動時にこのPINに接続されているボタンの状態を検査し、押下されていたら未登録状態に初期化する
constexpr uint8_t reset_button_pin = 41;

}  // namespace

/*
 * NVSに保存してある共有鍵をロードする
 * 共有鍵がない場合は未登録デバイスとしてふるまう
 */
bool
prepare_secret() {
	Preferences prefs{};
	if (!prefs.begin(prefs_name)) {
		Serial.println("Failed to init prefs");
		return false;
	}
	ble_uuid128_t stored;
	// 起動時に初期化ボタンが押されておらず、かつ保存されているUUIDが指定されたUUIDと同一ならば保存されていた共有鍵を使用する
	// 初期化ボタンが押されている場合は登録されていた共有鍵を使用せず、未登録デバイスとして動作を開始する
	if (digitalRead(reset_button_pin) == HIGH && prefs.getBytes(prefs_uuid, stored.value, sizeof(stored.value)) == UUID_SIZE) {
		stored.u.type = BLE_UUID_TYPE_128;
		if (my_uuid == NimBLEUUID(&stored)) {
			Serial.println("UUID not changed");
			std::array<std::byte, Sesame::SECRET_SIZE> secret;
			if (prefs.getBytes(prefs_secret, secret.data(), secret.size()) != secret.size() || !server.set_registered(secret)) {
				Serial.printf("Failed to restore secret\n");
				return false;
			}
			Serial.printf("Restored secret = %s\n", util::bin2hex(secret).c_str());
		}
	}
	return true;
}

/*
 * スマホで登録処理が完了した際のコールバック
 */
void
on_registration(const NimBLEAddress& addr, const std::array<std::byte, Sesame::SECRET_SIZE>& secret) {
	Serial.printf("registration by %s, secret=%s\n", addr.toString().c_str(), util::bin2hex(secret).c_str());
	Preferences prefs{};
	if (!prefs.begin(prefs_name)) {
		Serial.println("Failed to init prefs");
		return;
	}
	if (prefs.putBytes(prefs_uuid, my_uuid.getValue(), UUID_SIZE) != UUID_SIZE) {
		Serial.println("Failed to store UUID, abort");
		return;
	}
	if (prefs.putBytes(prefs_secret, secret.data(), secret.size()) != secret.size()) {
		Serial.println("Failed to store secret");
		return;
	}
	Serial.println("Secret stored");
}

/*
 * Remote / Remote nano / Open Sensorからのコマンド受信時コールバック
 */
Sesame::result_code_t
on_command(NimBLEAddress addr,
           Sesame::item_code_t cmd,
           const std::string& tag,
           std::optional<libsesame3bt::trigger_type_t> trigger_type) {
	Serial.printf("receive command = %u (%s: %s) from %s\n", static_cast<uint8_t>(cmd),
	              trigger_type.has_value() ? std::to_string(static_cast<uint8_t>(*trigger_type)).c_str() : "str", tag.c_str(),
	              addr.toString().c_str());
	if (cmd == Sesame::item_code_t::lock || cmd == Sesame::item_code_t::unlock) {
		bool locked = cmd == Sesame::item_code_t::lock;
		taskManager.schedule(onceSeconds(0), [locked]() { server.send_lock_status(locked); });
	}
	return Sesame::result_code_t::success;
}

void
setup() {
	delay(5000);
	Serial.begin(115200);

	if (!prepare_secret()) {
		return;
	}
	Serial.printf("my uuid = %s\n", my_uuid.toString().c_str());
	if (!server.is_registered()) {
		server.set_on_registration_callback(on_registration);
	}
	server.set_on_command_callback(on_command);
	if (!server.begin(Sesame::model_t::sesame_5, my_addr, my_uuid)) {
		Serial.println("initialization failed");
		return;
	}
	auto addr = NimBLEDevice::getAddress();
	Serial.printf("my address = %s(%u)\n", addr.toString().c_str(), addr.getType());
	if (!server.start_advertising()) {
		Serial.println("Start advertisement failed");
		return;
	}
	initialized = true;
	Serial.printf("Advertisement started in %s state\n", server.is_registered() ? "Registered" : "NOT Registered");
}

uint32_t last_reported;

void
loop() {
	if (!initialized) {
		delay(1000);
		return;
	}
	if (last_reported == 0 || millis() - last_reported > 3'000) {
		if (!server.is_registered()) {
			Serial.println("NOT Registered");
		} else {
			Serial.printf("session count = %u\n", server.get_session_count());
		}
		last_reported = millis();
	}
	server.update();
	taskManager.runLoop();
	delay(100);
}
