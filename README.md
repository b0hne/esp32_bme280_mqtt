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