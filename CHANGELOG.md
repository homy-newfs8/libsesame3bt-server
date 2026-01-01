# Changelog

# [v0.10.0] 2026-01-XX
- Add `set_auto_send_flags()` and `set_on_login_callback()` (see example/peripheral for usage).
- Add `scaled_voltage` parameter to `command_callback_t` (breaking change).

## [v0.9.1] 2025-12-13
- Do not drop UUID history tag with extra data. Extra data is not handled yet (may be battery voltages).

## [v0.9.0] 2025-11-05
- Bump libsesame3bt-core
- Use renamed type `history_tag_type_t`.

## [v0.8.0] 2025-10-25
- Set in_unlock flag properly in `send_lock_status()`, also modified some status values.
- Remove unintended Arduino dependency.

## [v0.7.0] 2025-09-30
- Add connect/disconnect callback.
- Add send_mecha_status().

## [v0.6.0] 2025-09-15
- Remove server_address parameter from SesameServer::begin(). The address will be calculated automatically.

## [v0.5.0] 2025-09-07
- Bump libsesame3bt-core version to 0.13.0

## [v0.4.0] 2025-08-09
- Supports Arduino ESP32 3.x in addition to 2.x.
- Bump NimBLE Arduino version to 2.3.3
- Remove TaskManagerIO dependency in example code (currently not supporting Arduino 3.x)

## [0.3.0] 2025-06-08
- Add `uuid_to_ble_address()`

### Breaking changes
- Add `trigger_type` argument to `on_command` callback.
Support for new Sesame history tag format (May 2025).
	- If `trigger_type` has a value, the `tag` string will be a UUID (128-bit) hex string.
The human-readable tag string value appears to be managed by the SESAME Server (SESAME Biz).
	 - Touch/Remote with older firmware will send the literal tag string as before. In that case, `trigger_value` will not have a value.

## [0.2.0] 2025-05-31
- Add `has_session()` and `disconnect()`.

## [0.1.3] 2025-04-03
- Bump NimBLE-Arduino version to 2.2.3

## [0.1.2] 2025-03-21
- Bump libsesame3bt-core version to 0.9.0

## [0.1.1] 2024-12-30

### Bug fix
- Properly update advertisement data when registered

## [0.1.0] 2024-12-29

- Initial release
