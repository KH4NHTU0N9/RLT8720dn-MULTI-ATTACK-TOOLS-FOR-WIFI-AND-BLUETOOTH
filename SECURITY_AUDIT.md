# Security, Concurrency & Firmware Audit Report

Target Repository: `KH4NHTU0N9/RLT8720dn-MULTI-ATTACK-TOOLS-FOR-WIFI-AND-BLUETOOTH`

## 1. Executive Summary

Following the initial security remediation pass (`SEC-01` through `SEC-12`), a second full-codebase audit and cross-validation against Realtek AmebaD SDK `3.1.9` uncovered additional critical boot-crash, RTOS deadlock, memory/heap fragmentation, 802.11/BLE framing, and HTTP protocol defects.

The codebase was refactored into a modular `src/` architecture and verified with both **Realtek AmebaD Arduino CLI** (`--warnings all`) and **4 host-side C++17 ASan + UBSan test suites (355 assertions total)**.

---

## 2. Initial Security Findings (SEC-01 .. SEC-12)

| ID | Severity | CWE | Summary & Remediation |
| :--- | :--- | :--- | :--- |
| SEC-01 | Critical | CWE-121 / CWE-770 | Replaced unbounded VLA in `/blewrite` with fixed `val[MAX_BLE_WRITE_BYTES]` (512 B) + `decodeHexPayload`. |
| SEC-02 | High | CWE-79 | Streamed HTML escaping (`& < > " '`) via `WebStream::writeHtmlEscaped` and `htmlEscape` across AP SSIDs, BLE names/MACs, and toast banners. |
| SEC-03 | High | CWE-125 / CWE-190 | Replaced unchecked BLE AD indexing with bounds-checked `parseBleDeviceName()` and `snprintf`. |
| SEC-04 | High | CWE-193 / CWE-400 | Replaced in-place SDK SSID NUL-termination with non-mutating `sanitizeSsid()` and capped scans at `MAX_SCAN_RESULTS` (64). |
| SEC-05 | High | CWE-400 / CWE-770 | Enforced `MAX_HTTP_REQ_SIZE` (`6144` B), fixed-size `req_buf`, case-insensitive `Content-Length` parser, and `413` / `400` rejections. |
| SEC-06 | Medium | CWE-113 / RFC 9112 | Standardized CRLF HTTP headers, RFC 9110 `303 See Other` redirects, `urlDecode()` with `%00` blocking, and GET vs POST enforcement (`405` / `404`). |
| SEC-07 | Medium | CWE-362 / CWE-416 | Replaced shared unsynchronized RX writes with lock-free staging and `_reset_all_targets()` cleanup. |
| SEC-08 | Medium | CWE-121 / CWE-476 | Guarded `rltk_wlan_running(0)`, null pointers, min `< 24` / max `> 1536` frame bounds, and SSID copy bounds. |
| SEC-09 | Medium | CWE-798 / CWE-306 | Documented default credential rotation warning; eliminated idle `wext_set_channel` polling overhead; Web UI Auth/CSRF remains a deployment hardening item. |
| SEC-10 | Low | CWE-120 | Replaced `sprintf` calls in OLED and MAC formatters with bounded `snprintf`. |
| SEC-11 | Low | CWE-400 / CWE-120 | Bounded `_networks` at `MAX_SPAM_NETWORKS` (64) and checked exact remaining beacon frame capacity (`MAX_BEACON_SIZE`). |
| SEC-12 | Low | CWE-20 | Added `[0, 100]` RSSI percentage clamp and complete `BLE.end()` resource cleanup after BLE operations. |

---

## 3. Comprehensive Firmware & Protocol Bugs Fixed (FW-01 .. FW-15)

| ID | Category | Root Cause & Verified Fix |
| :--- | :--- | :--- |
| FW-01 | Critical Boot Crash | `oled_display.cpp`: `_i2c_init()` called `digitalWrite(OLED_SDA, LOW)` before `pinMode(OLED_SDA, OUTPUT)`, dereferencing unallocated `gpio_pin_struct[8]` inside AmebaD `wiring_digital.c` and triggering an early HardFault boot loop prior to `WiFi.apbegin()`. Fixed by calling `pinMode()` before any `digitalWrite()` and switching `_sda_high()` to `INPUT_PULLUP`. |
| FW-02 | RTOS Deadlock & RX Level | `interference.cpp`: Calling `wifi_set_promisc(0, NULL, 0)`, `malloc`, or GPIO/Serial APIs inside `_beacon_sniffer_cb` risked self-deadlock on non-recursive `promisc_sema`. Also fixed wrong level (`2` -> `3` `RTW_PROMISC_ENABLE_2` with `len_used = 1`). |
| FW-03 | Beacon Tag Truncation | `interference.cpp`: Sniffer checked fixed `offset = 4/8` and truncated tags at arbitrary 400 B. Added `interference_sanitize_beacon_tags()` TLV boundary walker and fixed Frame Control / BSSID matching (`frame[0] == 0x80`, `&frame[16]`). |
| FW-04 | Synthetic Band Mismatch | `interference.cpp`: Fixed 2.4 GHz vs 5 GHz IE leakage (no Tag 3 DS Param on 5 GHz; proper HT Cap + HT Op primary channel byte; non-DFS 5 GHz ghost/adjacent channels). |
| FW-05 | 802.11 Framing & NAV | `packet-injection.h/.cpp`: Fixed `NullFrame` `0x48` erroneously carrying a QoS Control field (`24` B exact), fixed `CSAFrame` packing (`31` B), clamped NAV to `0x7FFF` (reserved bit 15 = 0 per IEEE 802.11-2020), fixed ECSA Tag 60 length `4` + IEEE operating classes (`81/115/118/121/124`), and stamped non-zero `micros()` TSF in beacons. |
| FW-06 | BLE Advertisement & GATT | `ble_manager.cpp`: Fixed Apple Proximity payload TLV length mismatch (exact 30 bytes matching AD2 length `0x1A`), replaced `random(0, 255)` off-by-one with `random(256)`, removed duplicate 3500 ms scan sleep, added `BLE.configClient()`, async `getConnId()` polling, `GAP_REMOTE_ADDR_LE_RANDOM` support, and detailed `BleWriteStatus` reporting. |
| FW-07 | LwIP Pool & Heap Churn | `web_stream.h`: Replaced ~400 tiny `client.print()` calls and linear `String` reallocations with a chunked `1024` B `WebStream` buffer matching `MEMP_NUM_TCP_SEG = 40`. |
| FW-08 | OLED Dirty-Page Diff | `oled_display.cpp`: Added page-level `memcmp` diffing (`_prev_fb`) and cached GPIO states to eliminate redundant full-panel bit-bang updates, fixed Y-axis separator gap (`y=14`), and prevented horizontal text clipping (`G. FastPair`, `Win SwiftPair`). |
| FW-09 | Input & Channel Consistency | `security_utils.h` & `web_router.cpp`: Replaced `String::toInt()` with `parseStrictIntRange()`, aligned `is24GhzChannel` (`1..14`) / `is5GhzChannel` (`> 14`) across scanner/UI/runner, implemented clean REST status codes (`400`/`404`/`405`/`413`), and fixed Early Idle interference recovery restarting `server.begin()`. |

---

## 4. Verification Matrix

