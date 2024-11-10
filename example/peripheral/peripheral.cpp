#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Sesame.h>
#include <SesameServer.h>
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
 * 3を超えるデバイスを扱う場合NimBLE-Arduinoの最大接続数も調整する必要があります
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
bool registered;

namespace {

constexpr size_t UUID_SIZE = 16;

// このサンプルではNVS領域にPreferencesを使って秘密情報を保存しています
constexpr const char prefs_name[] = "sesameserver";
constexpr const char prefs_privkey[] = "privk";
constexpr const char prefs_uuid[] = "uuid";
constexpr const char prefs_secret[] = "secret";
// 起動時にこのPINに接続されているボタンの状態を検査し、押下されていたら未登録状態に初期化する
constexpr uint8_t reset_button_pin = 41;

}  // namespace

/*
 * キーペアを用意する
 * NVSに秘密鍵が保存してあればそれをロードし、なければ新規に生成する
 *
 * NVSの秘密鍵をロードした場合は、さらに保存してある共有鍵をロードする
 * 共有鍵がない場合は未登録デバイスとしてふるまう
 */
bool
prepare_keypair() {
	Preferences prefs{};
	if (!prefs.begin(prefs_name)) {
		Serial.println("Failed to init prefs");
		return false;
	}
	bool need_recreate = true;
	ble_uuid128_t stored;
	// 初期化ボタンが押されておらず、かつ保存されているUUIDが指定されたUUIDと同一ならば保存されていたキーペアを使用する
	if (digitalRead(reset_button_pin) == HIGH && prefs.getBytes(prefs_uuid, stored.value, sizeof(stored.value)) == UUID_SIZE) {
		stored.u.type = BLE_UUID_TYPE_128;
		if (my_uuid == NimBLEUUID(&stored)) {
			Serial.println("UUID not changed");
			need_recreate = false;
		}
	}
	std::array<std::byte, Sesame::SK_SIZE> privkey;
	if (need_recreate) {
		if (prefs.putBytes(prefs_uuid, my_uuid.getValue(), UUID_SIZE) != UUID_SIZE) {
			Serial.println("Failed to store UUID, abort");
			return false;
		}
	} else {
		if (prefs.getBytes(prefs_privkey, privkey.data(), privkey.size()) == privkey.size()) {
			if (server.load_key(privkey)) {
				Serial.println("keypair loaded");
				std::array<std::byte, Sesame::SECRET_SIZE> secret;
				if (prefs.getBytes(prefs_secret, secret.data(), secret.size()) == secret.size()) {
					if (server.set_registered(secret)) {
						Serial.printf("Stored secret=%s\n", util::bin2hex(secret).c_str());
						registered = true;
					}
				}
				return true;
			} else {
				Serial.println("Failed to load keypair, abort");
				return false;
			}
		} else {
			Serial.println("Stored keys not valid, regenerate");
		}
	}
	if (!server.generate_keypair()) {
		Serial.println("Failed to generate keypair, abort");
		return false;
	}
	std::array<std::byte, Sesame::PK_SIZE> pubkey;
	if (!server.export_keypair(pubkey, privkey)) {
		Serial.println("Failed to retrieve keypair, abort");
		return false;
	}
	if (prefs.putBytes(prefs_privkey, privkey.data(), privkey.size()) == privkey.size()) {
		Serial.println("Keypair generated, and stored");
		if (prefs.isKey(prefs_secret)) {
			if (!prefs.remove(prefs_secret)) {
				Serial.println("Failed to delete old shared secret");
				return false;
			}
		}
	} else {
		Serial.println("Failed to store keypair, abort");
	}
	return true;
}

void
on_registration(NimBLEAddress addr, const std::array<std::byte, Sesame::SECRET_SIZE> secret) {
	Serial.printf("registration by %s, secret=%s\n", addr.toString().c_str(), util::bin2hex(secret).c_str());
	Preferences prefs{};
	if (!prefs.begin(prefs_name)) {
		Serial.println("Failed to init prefs");
		return;
	}
	if (prefs.putBytes(prefs_secret, secret.data(), secret.size()) != secret.size()) {
		Serial.println("Failed to store secret");
		return;
	}
	Serial.println("Secret stored");
}

Sesame::result_code_t
on_command(NimBLEAddress addr, Sesame::item_code_t cmd, const std::string& tag) {
	Serial.printf("receive command = %u from %s\n", static_cast<uint8_t>(cmd), addr.toString().c_str());
	return Sesame::result_code_t::success;
}

void
setup() {
	delay(5000);
	Serial.begin(115200);
	if (!NimBLEDevice::init("Peripheral Demo") || !NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM) ||
	    !NimBLEDevice::setOwnAddr(my_addr)) {
		Serial.println("Failed to init BLE");
		return;
	}

	initialized = prepare_keypair();
	if (!initialized) {
		return;
	}
	Serial.printf("my uuid = %s\n", my_uuid.toString().c_str());
	auto addr = NimBLEDevice::getAddress();
	Serial.printf("my address = %s(%u)\n", addr.toString().c_str(), addr.getType());
	if (!registered) {
		server.set_on_registration_callback(on_registration);
	}
	server.set_on_command_callback(on_command);
	initialized = server.begin(Sesame::model_t::sesame_5, my_uuid);
	if (!initialized) {
		Serial.println("initialization failed");
		return;
	}
	initialized = server.start_advertising();
	if (!initialized) {
		Serial.println("Advertisement failed");
		return;
	}
	Serial.println("Advertisement started");
}

uint32_t last_reported;

void
loop() {
	if (!initialized) {
		delay(1000);
		return;
	}
	if (last_reported == 0 || millis() - last_reported > 3'000) {
		Serial.printf("session count = %u\n", server.get_session_count());
		last_reported = millis();
	}
	server.update();
	delay(100);
}
