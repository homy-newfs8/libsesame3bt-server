#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Sesame.h>
#include <SesameServer.h>
#include <libsesame3bt/ClientCore.h>
#include <libsesame3bt/util.h>
#include <mutex>
#if __has_include("mysesame-config.h")
#include "mysesame-config.h"
#endif

/*
 * サーバーはUUIDから生成されたBLE Addressで動作します。
 */
/* SESAME DEVICE UUID in format 12345678-90ab-cdef-1234-567890abcdef */
#ifndef SESAME_SERVER_UUID
#define SESAME_SERVER_UUID "***Replace this***"
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
using libsesame3bt::core::Status;
namespace util = libsesame3bt::core::util;

NimBLEUUID my_uuid{SESAME_SERVER_UUID};

SesameServer server{SESAME_SERVER_MAX_SESSIONS};

bool initialized;

std::mutex status_mutex;
bool status_updated;
bool status;

namespace {

constexpr size_t UUID_SIZE = 16;

// このサンプルではNVS領域にPreferencesを使って秘密情報を保存しています
constexpr const char prefs_name[] = "sesameserver";
constexpr const char prefs_uuid[] = "uuid";
constexpr const char prefs_secret[] = "secret";
// 起動時にこのPINに接続されているボタンの状態を検査し、押下されていたら未登録状態に初期化する
constexpr uint8_t reset_button_pin = 41;

}  // namespace

static const uint8_t deny_address[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

/*
 * 接続元デバイスのBLEアドレスを見て接続を拒否する
*/
bool
check_address(const NimBLEAddress& addr) {
	if (addr == NimBLEAddress(deny_address, BLE_ADDR_RANDOM)) {
		Serial.println("connection rejected");
		return false;
	}
	return true;
}

/*
 * NVSに保存してある共有鍵をロードする
 * 共有鍵がない場合は未登録デバイスとしてふるまう
 */
bool
prepare_secret() {
#ifdef SESAME_SERVER_SECRET
	std::array<std::byte, Sesame::SECRET_SIZE> secret;
	return util::hex2bin(SESAME_SERVER_SECRET, secret) && (server.set_registered(secret), true);
#else
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
#endif
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
           std::optional<libsesame3bt::history_tag_type_t> trigger_type,
           float scaled_voltage,
           float scaled_voltage2,
           std::string_view extra) {
	Serial.printf(
	    "receive command = %u (%s: %s) from %s, svolt=%s, svolt2=%s, pct=%s, pct(opensensor)=%s\n", static_cast<uint8_t>(cmd),
	    trigger_type.has_value() ? std::to_string(static_cast<uint8_t>(*trigger_type)).c_str() : "str", tag.c_str(),
	    addr.toString().c_str(), isnan(scaled_voltage) ? "N/A" : String(scaled_voltage, 2).c_str(),
	    isnan(scaled_voltage2) ? "N/A" : String(scaled_voltage2, 2).c_str(),
	    isnan(scaled_voltage) ? "N/A" : String(Status::scaled_voltage_to_pct(scaled_voltage, Sesame::model_t::sesame_5), 2).c_str(),
	    isnan(scaled_voltage) ? "N/A"
	                          : String(Status::scaled_voltage_to_pct(scaled_voltage, Sesame::model_t::open_sensor_1), 2).c_str());
	if (extra.size() > 0) {
		Serial.printf("extra = %s\n", libsesame3bt::core::util::bin2hex(extra.data(), extra.size()).c_str());
	} else {
		Serial.println("extra = N/A");
	}
	if (cmd == Sesame::item_code_t::lock || cmd == Sesame::item_code_t::unlock) {
		std::lock_guard<std::mutex> lock(status_mutex);
		status = cmd == Sesame::item_code_t::lock;
		status_updated = true;
	}
	return Sesame::result_code_t::success;
}

void
setup() {
	Serial.begin(115200);
	delay(5000);

	if (!prepare_secret()) {
		return;
	}
	auto addr = SesameServer::uuid_to_ble_address(my_uuid);
	if (addr.isNull()) {
		Serial.println("Failed to convert UUID to BLE address");
		return;
	}
	Serial.printf("my uuid = %s, addr = %s\n", my_uuid.toString().c_str(), addr.toString().c_str());
	if (!server.is_registered()) {
		server.set_on_registration_callback(on_registration);
	}
	server.set_on_command_callback(on_command);
	// ログイン完了時にmecha_statusを送信するコールバックを設定
	server.set_on_login_callback([](const NimBLEAddress& addr) {
		Serial.printf("login from: %s, sending mecha_status\n", addr.toString().c_str());
		if (!server.send_mecha_status(&addr, Sesame::mecha_status_5_t{})) {
			Serial.println("Failed to send mecha_status");
		}
	});
	// 上記のon_loginコールバックでmecha_statusを送信するため、mecha_statusの自動送信は無効にする
	server.set_auto_send_flags(libsesame3bt::auto_send::flags::mecha_setting);
	server.set_connect_check_callback(check_address);
	if (!server.begin(Sesame::model_t::sesame_5, my_uuid)) {
		Serial.println("initialization failed");
		return;
	}
	addr = NimBLEDevice::getAddress();
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
	{
		std::lock_guard<std::mutex> lock(status_mutex);
		if (status_updated) {
			status_updated = false;
			server.send_lock_status(status);
		}
	}
	delay(100);
}
