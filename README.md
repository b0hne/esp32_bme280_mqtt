# ESP32 BME280 MQTT Sensor (ESP-IDF)

Reads temperature, pressure and humidity from a Bosch BME280 over I2C  
and publishes JSON data to an MQTT broker.

Designed for ESP-IDF 5.x.

---

## Features

- WiFi (STA mode, configured via `menuconfig`)
- MQTT publish (auto reconnect)
- Home Assistant MQTT Discovery support
- Forced-mode BME280 measurement
- Clean component structure (`net_mqtt` separated)

---

## Hardware

- ESP32 (tested with ESP32-C6)
- BME280 (I2C)
- Default I2C pins:
  - SDA: GPIO 5
  - SCL: GPIO 6
  - Address: `0x76`

---

## MQTT Payload

Published to:


CONFIG_MQTT_STATE_TOPIC


Example:

```json
{
  "temperature": 21.49,
  "pressure": 1013.25,
  "humidity": 45.00
}

Pressure is in hPa.

Configuration

Run:

idf.py menuconfig

Set:

WiFi and MQTT Configuration

WiFi SSID

WiFi password

MQTT URI (example: mqtt://homepi.fritz.box)

MQTT credentials (optional)

MQTT state topic

Home Assistant discovery enable

Secrets are not committed (sdkconfig is ignored).

Build & Flash
idf.py build
idf.py flash monitor
Home Assistant

If discovery is enabled, sensors are created automatically under:

homeassistant/sensor/<device_id>_*/config

Entities:

Temperature

Pressure

Humidity

Project Structure
components/net_mqtt/   → WiFi + MQTT handling
main/                  → BME280 logic
License

MIT (or choose your preferred license)


---

If you want, I can also generate:

- a small architecture diagram (Markdown + ASCII)
- a CI workflow for GitHub Actions
- a Home Assistant example screenshot section
- or a more “portfolio-style” README for showcasing on GitHub