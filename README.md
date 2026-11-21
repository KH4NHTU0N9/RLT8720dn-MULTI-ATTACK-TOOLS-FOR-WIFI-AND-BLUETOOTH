# RTL8720DN Multi-Tool & Diagnostics Suite (Wi-Fi 2.4G/5G & BLE)

![Platform](https://img.shields.io/badge/Platform-Arduino%20IDE-blue)
![Hardware](https://img.shields.io/badge/Hardware-RTL8720DN%20(BW16)-orange)
![Tests](https://img.shields.io/badge/Host%20Tests-355%20Assertions%20ASan%2FUBSan-brightgreen)

Dự án mã nguồn mở dành cho module **RTL8720DN (BW16 Dual-Band)**, được thiết kế theo kiến trúc module hóa (`src/`) để kiểm thử, chẩn đoán và mô phỏng giao thức không dây trên cả hai băng tần Wi-Fi 2.4GHz / 5GHz và Bluetooth Low Energy (BLE), tích hợp màn hình OLED SSD1306 và giao diện Web UI tại `http://192.168.1.1`.

---

## Kiến trúc mã nguồn (Modular Architecture)

Toàn bộ firmware trong `RTL8720dn-Deauther/` được tách thành các module độc lập, rõ trách nhiệm và dễ kiểm thử:

* `RTL8720dn-Deauther.ino`: Entrypoint gọn nhẹ (`setup()` / `loop()`), khôi phục SoftAP và điều phối state machine.
* `debug.h`: Macro ghi log UART (`115200` baud) và `debugPrintMac()` không cấp phát heap.
* `src/config/app_config.h`: Trung tâm cấu hình hằng số dung lượng, giới hạn tham số và timing (tối ưu cho ~250 KB FreeRTOS heap).
* `src/core/`: State dùng chung (`app_state.h`), điều khiển RGB LED (`led_controller.{h,cpp}`), và vòng lặp tác vụ radio (`attack_runner.{h,cpp}`).
* `src/oled/oled_display.{h,cpp}`: Driver SSD1306 `128x64` Software I2C (PA25/PA26) với thứ tự `pinMode` trước `digitalWrite`, cache trạng thái chân GPIO và cơ chế **dirty-page diffing**.
* `src/wifi/wifi_scanner.{h,cpp}`: Quét Wi-Fi 2.4GHz + 5GHz an toàn bộ nhớ, phân loại băng tần đồng nhất (`1..14` vs `> 14`) và khôi phục SoftAP (`wifiRestoreSoftAp`).
* `src/ble/ble_manager.{h,cpp}`: Quản lý BLE Scanner, GATT Client (`configClient()` + async connection polling), và bộ dựng gói quảng bá chuẩn độ dài AD TLV (`Apple 30B`, `Google 14B`, `Windows 18B`).
* `src/packet-injection/packet-injection.{h,cpp}` & `src/spammer/spammer.{h,cpp}` & `src/interference/interference.{h,cpp}`: Bộ dựng khung 802.11 (`__attribute__((packed))`), kiểm tra giới hạn TLV tag, staging lock-free giữa callback RX và vòng lặp chính để tránh deadlock semaphore `wifi_set_promisc`.
* `src/security/security_utils.h`: Escape HTML chống XSS, URL decode chặn `%00`, parser số nguyên chặt chẽ (`parseStrictIntRange`), parser HTTP request-line và `Content-Length` không phân biệt hoa/thường.
* `src/web/`: `web_stream.h` (gộp chunk `<= 1024B` bảo vệ pool TCP LwIP), `web_ui.{h,cpp}` (giao diện Web 4 tab responsive), và `web_router.{h,cpp}` (định tuyến GET/POST chuẩn HTTP 200/204/303/400/404/405/413).

---

## Giao diện Web UI & Cách sử dụng

1. Sau khi cấp nguồn hoặc nhấn **RST**, mạch phát mạng SoftAP mặc định:
   * **SSID:** `RTL8720dn-Deauther`
   * **Mật khẩu:** `0123456789` *(Khuyến nghị đổi trong `src/config/app_config.h` trước khi triển khai thực tế)*
2. Kết nối điện thoại hoặc máy tính vào Wi-Fi trên, sau đó mở trình duyệt tại **`http://192.168.1.1`**.
3. Giao diện mới được tổ chức thành **4 tab trực quan**:
   * **Wi-Fi:** Tách riêng danh sách mạng 5 GHz và 2.4 GHz, nút chọn nhanh từng băng tần (`All`), thông báo toast trạng thái và nút submit tách biệt rõ ràng cho từng chế độ.
   * **Beacon Flood:** Hỗ trợ nhập danh sách SSID, preset mẫu, hoặc sinh SSID ngẫu nhiên có kiểm soát dung lượng (`1..64`).
   * **Bluetooth:** Chọn cấu hình BLE Advertising hoặc quét thiết bị BLE lân cận và điền nhanh MAC/Address Type (`Public`/`Random`) xuống form GATT Writer.
   * **Settings:** Cấu hình thông số mặc định, bật/tắt LED trên board và theo dõi bộ nhớ Free Heap theo thời gian thực.

---

## Yêu cầu phần cứng & Biên dịch (Hardware & Build)

* **Board:** Ai-Thinker BW16 (Realtek RTL8720DN).
* **Core Arduino:** Realtek AmebaD SDK `3.1.9` (`realtek:AmebaD:Ai-Thinker_BW16`).
* **Ăng-ten ngoài:** Chuẩn IPEX hỗ trợ dual-band 2.4G/5GHz.

Biên dịch kiểm tra qua Arduino CLI:

```sh
arduino-cli compile --clean --fqbn realtek:AmebaD:Ai-Thinker_BW16 --warnings all RTL8720dn-Deauther
```

*(Khi nạp code trên macOS/Arduino IDE với BW16, nhấn giữ nút **BURN**, bấm nhả **RST**, rồi thả **BURN** để vào UART Download Mode ở baudrate chuẩn).*

---

## Kiểm thử tự động Host-Side (ASan & UBSan)

Dự án cung cấp bộ mock phần cứng AmebaD (`tests/mock/`) và **4 suite unit test C++17** chạy trực tiếp với `-Wall -Wextra -Werror -fsanitize=address,undefined` (**355 assertions**):

```sh
mkdir -p tests/bin

# 1. Security helpers, HTTP & string parsers (102 assertions)
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Itests/mock -IRTL8720dn-Deauther/src/security \
  tests/test_security_fixes.cpp -o tests/bin/test_security_fixes && \
  ./tests/bin/test_security_fixes

# 2. OLED Software I2C GPIO safety & dirty-page diffing (32 assertions)
clang++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -fsanitize=address,undefined \
  -Itests/mock -IRTL8720dn-Deauther/src \
  tests/test_oled_display.cpp RTL8720dn-Deauther/src/oled/oled_display.cpp \
  -o tests/bin/test_oled_display && ./tests/bin/test_oled_display

# 3. Packet framing, Spammer, Interference sniffer safety, LED & BLE (129 assertions)
clang++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -fsanitize=address,undefined \
  -Itests/mock -IRTL8720dn-Deauther/src \
  tests/test_rf_and_ble.cpp \
  RTL8720dn-Deauther/src/packet-injection/packet-injection.cpp \
  RTL8720dn-Deauther/src/spammer/spammer.cpp \
  RTL8720dn-Deauther/src/interference/interference.cpp \
  RTL8720dn-Deauther/src/core/led_controller.cpp \
  RTL8720dn-Deauther/src/ble/ble_manager.cpp \
  RTL8720dn-Deauther/src/wifi/wifi_scanner.cpp \
  RTL8720dn-Deauther/src/oled/oled_display.cpp \
  RTL8720dn-Deauther/src/core/attack_runner.cpp \
  -o tests/bin/test_rf_and_ble && ./tests/bin/test_rf_and_ble

# 4. WebStream chunking, XSS rendering & HTTP Router (92 assertions)
clang++ -std=c++17 -Wall -Wextra -Werror -DUNIT_TEST -fsanitize=address,undefined \
  -Itests/mock -IRTL8720dn-Deauther/src \
  tests/test_web_and_router.cpp \
  RTL8720dn-Deauther/src/web/web_ui.cpp \
  RTL8720dn-Deauther/src/web/web_router.cpp \
  RTL8720dn-Deauther/src/core/led_controller.cpp \
  RTL8720dn-Deauther/src/oled/oled_display.cpp \
  RTL8720dn-Deauther/src/spammer/spammer.cpp \
  RTL8720dn-Deauther/src/interference/interference.cpp \
  RTL8720dn-Deauther/src/packet-injection/packet-injection.cpp \
  RTL8720dn-Deauther/src/ble/ble_manager.cpp \
  RTL8720dn-Deauther/src/wifi/wifi_scanner.cpp \
  RTL8720dn-Deauther/src/core/attack_runner.cpp \
  -o tests/bin/test_web_and_router && ./tests/bin/test_web_and_router
