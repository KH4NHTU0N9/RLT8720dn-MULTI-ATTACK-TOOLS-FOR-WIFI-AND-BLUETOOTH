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
