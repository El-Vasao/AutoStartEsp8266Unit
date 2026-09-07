# Dependency RAM audit (ESP8266)

## Goal

Verify which dependencies are truly used by the project and which framework objects are only compiled but not linked into the final firmware.

## Findings

- `ESPAsyncWebServer` is used directly in web subsystem and is required.
- `ESPAsyncTCP` is a transitive dependency of `ESPAsyncWebServer` on ESP8266; explicit duplicate pin was removed from `lib_deps`.
- `DallasTemperature` is used by `SensorsController`; `OneWire` is transitively required by Dallas and no longer duplicated in `lib_deps`.
- Vendored `JsonStreamingParser` (under `lib/JsonStreamingParser`) is used in config/program parsing; no external `JsonStreamingParser` dependency is required.

## Framework-level compilation vs linking

During PlatformIO build, framework units such as:

- `ESP8266WiFi/BearSSLHelpers.cpp`
- `ESP8266WiFi/CertStoreBearSSL.cpp`

can still be compiled as part of `framework-arduinoespressif8266`.

This does not imply those symbols are linked into the final firmware image.
Automated footprint report (`dist/ram-footprint-release.json`) now checks linked symbols explicitly.
Current result: `secure_symbols_detected: []` (no linked `BearSSL` / `WiFiClientSecure` / `CertStore` markers).

## Practical policy

1. Keep `lib_deps` minimal and avoid duplicate direct+transitive entries.
2. Use link-map / ELF symbol checks for optimization decisions, not compile log lines alone.
3. Treat framework objects as optimization candidates only when they appear in final linked symbols/sections.
