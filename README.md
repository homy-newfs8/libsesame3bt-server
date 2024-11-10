# libsesame3bt-server
BLE上でSESAME 5っぽく見えるような動作をするESP32用ライブラリ

本ライブラリはBLE上でSESAME 5デバイスを模倣した動作をすることにより、CANDYHOUSE Remote / Remote nano / SESAME Touch のボタン操作およびOpen Sensorの状態変化を受信することができるライブラリです。
ESP32シリーズで動作します。

# 必要なもの
- 適切なUUIDとBLEアドレスの組<br/>
不適切なUUIDとBLEアドレスの組を使用すると、Remote等のデバイスが接続してこないようです。私は故障したSESAME 5のUUIDとアドレスを使いました。
- ESP32シリーズ<br/>
基本的に[Arduino core for ESP32](https://github.com/espressif/arduino-esp32)がサポートするESP32シリーズはどれでも動作すると思われます。私はArduino core v2.0系を使い、ESP32 C3やESP32 S3で動作確認しています。

# 使い方
[example/peripheral](example/peripheral/peripheral.cpp)を見てください。
