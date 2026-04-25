# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project type

This is an ESP-IDF Wi-Fi station example project named `station`, currently configured for `esp32s3` in local VS Code settings and in the existing build output.

## Common commands

Run these from the repository root after sourcing the ESP-IDF environment.

- Open project configuration: `idf.py menuconfig`
- Build the firmware: `idf.py build`
- Build, flash, and open serial monitor: `idf.py -p PORT flash monitor`
- Run the existing pytest-based test file: `pytest pytest_wifi_station.py`
- Run the single existing test only: `pytest pytest_wifi_station.py -k test_wifi_sdkconfig_disable_softap_save_binary_size`

## Test setup notes

- `pytest_wifi_station.py` uses `pytest-embedded` / `pytest-embedded-idf`.
- The existing test is marked `@pytest.mark.two_duts` and parametrized with `default|enable_softap`, so it expects two DUTs and compares binary sizes between two sdkconfig variants rather than exercising the main station connection flow on one board.

## Architecture overview

### Build structure

- Top-level `CMakeLists.txt` is the standard ESP-IDF project entry and enables `MINIMAL_BUILD`.
- The app logic lives entirely in the `main` component.
- `main/CMakeLists.txt` registers `station_example_main.c` and declares `esp_wifi`, `nvs_flash`, and `bt` as private requirements.
- `main/idf_component.yml` adds managed dependencies for `espressif/esp_wifi_remote` and `espressif/esp_hosted` when building for targets such as `esp32p4` or `esp32h2`.

### Runtime flow

The application is a small event-driven Wi-Fi STA example with a clear 3-layer structure:

1. `app_main()`
   - Initializes NVS.
   - Raises Wi-Fi log verbosity when configured.
   - Hands off to `wifi_init_sta()`.

2. `wifi_init_sta()`
   - Creates the FreeRTOS event group used for connection state.
   - Initializes `esp_netif`, creates the default event loop, and creates the default STA netif.
   - Initializes the Wi-Fi driver, registers event handlers, sets STA config, starts Wi-Fi, and blocks on connection success/failure bits.

3. `event_handler()`
   - On `WIFI_EVENT_STA_START`, calls `esp_wifi_connect()`.
   - On `WIFI_EVENT_STA_DISCONNECTED`, retries until `CONFIG_ESP_MAXIMUM_RETRY` is reached, then sets the fail bit.
   - On `IP_EVENT_STA_GOT_IP`, logs the assigned IP, resets retry count, and sets the connected bit.

The important architectural pattern is that asynchronous Wi-Fi/IP events update a FreeRTOS event group, while the main init path waits synchronously on those bits.

## Configuration model

- `main/Kconfig.projbuild` defines the project-facing configuration in `Example Configuration`, including SSID, password, max retry count, WPA3 SAE mode, password identifier, and auth-mode threshold.
- The code declares macros mapped from Kconfig (`CONFIG_ESP_WIFI_SSID`, `CONFIG_ESP_WIFI_PASSWORD`, `CONFIG_ESP_MAXIMUM_RETRY`), so the intended configuration path is via `menuconfig` / `sdkconfig`.
- However, `main/station_example_main.c` currently hardcodes `.ssid = "Licoe"` and `.password = "@Li123456789"` inside `wifi_config_t`. If changing Wi-Fi credentials behavior, inspect both the Kconfig-backed macros and this hardcoded struct initialization.
- `sdkconfig.defaults` currently sets `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=n`, which matters because the pytest file compares builds with and without SoftAP support.

## Files worth reading first

- `main/station_example_main.c` — all runtime logic is here.
- `main/Kconfig.projbuild` — project configuration surface exposed in `menuconfig`.
- `pytest_wifi_station.py` — the only repo test, focused on binary size/config comparison.
- `README.md` — upstream example usage and expected serial output.

## Environment clues already present in the repo

- `.vscode/settings.json` points to ESP-IDF `v5.5.4`, uses `build` as the build directory, sets `IDF_TARGET` to `esp32s3`, and configures a Windows serial port (`COM5`) for local flashing.
- `build/project_description.json` also reflects an `esp32s3` build against ESP-IDF `v5.5.4`.

## What is not in this repo

- No repository-level Cursor rules, `.cursorrules`, Copilot instructions, or existing project `CLAUDE.md` were found.
- No project-specific lint command or lint configuration file was found in the repository; do not assume one exists without checking the active ESP-IDF environment or external tooling setup.
