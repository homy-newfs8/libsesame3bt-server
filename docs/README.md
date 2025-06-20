# libsesame3bt-server
BLE上でSESAME 5っぽく見えるような動作をするESP32用ライブラリ

本ライブラリはBLE上でSESAME 5デバイスを模倣した動作をすることにより、CANDY HOUSE Remote / Remote nano / SESAME Touch / SESAME Face の施錠開錠操作およびOpen Sensorの状態変化を受信することができるライブラリです。
ESP32シリーズで動作します。

# 必要なもの
- 適切なUUIDとBLEアドレスの組<br/>
SESAME 5以降の機種ではBLEアドレスとしてUUIDから算出可能な値を使う仕様になっています(Remote/Touch等は操作対象デバイスのUUIDを保持しており、操作時にはBTスキャンを実行せずに算出されたアドレスに接続します)。<br/>
サーバーで使用するUUIDとBLEアドレスの組を指定する場合は、まず適当なツールでUUIDを生成し、後に[example/uuid_to_btaddr](../example/uuid_to_btaddr/uuid_to_btaddr.cpp)を使用してBLEアドレスを算出して使用してください。
- ESP32シリーズ<br/>
基本的に[Arduino core for ESP32](https://github.com/espressif/arduino-esp32)がサポートするESP32シリーズはどれでも動作すると思われます。私はArduino core v2.0系を使い、ESP32 C3やESP32 S3で動作確認しています。

# 使い方
[example/peripheral](../example/peripheral/peripheral.cpp)を見てください。
ESPHomeの外部コンポーネントとして動作する[esphome-sesame_server](https://github.com/homy-newfs8/esphome-sesame_server)もあります。
