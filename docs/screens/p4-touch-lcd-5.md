---
title: "Waveshare ESP32-P4-WIFI6-Touch-LCD-5 Home Assistant Setup"
description:
  EspControl on the Waveshare ESP32-P4-WIFI6-Touch-LCD-5 - a 5-inch 1280x720 touchscreen with 15 cards, powered by ESP32-P4.
---

# 5-inch Waveshare ESP32-P4-WIFI6-Touch-LCD-5

The **Waveshare ESP32-P4-WIFI6-Touch-LCD-5** is a 5-inch 720 × 1280 touchscreen board powered by an **ESP32-P4** processor, with WiFi through an ESP32-C6 co-processor and a built-in speaker output. EspControl shows it as a **1280 × 720 landscape** panel by default; rotate it to 90° or 270° in [Display settings](/settings/display) for a portrait layout.

This firmware targets **ESP32-P4 v3.x production silicon**, which Waveshare uses for this board. If unsure, run `esptool --port YOUR_SERIAL_PORT chip-id` before installing. Physical device validation is still in progress.

## Card Grid

<!--@include: ../generated/screens/p4-touch-lcd-5-grid.md-->

In portrait the same 15 cards are arranged as 3 columns by 5 rows.

## Install

Connect a USB-C data cable to the board's **UART** USB-C port, then click the button below.

<!--@include: ../generated/screens/p4-touch-lcd-5-install.md-->

For a full walkthrough including WiFi setup and Home Assistant pairing, see the [Install guide](/getting-started/install).

## ESPHome Manual Setup

If you use ESPHome and prefer to compile firmware yourself:

```yaml
substitutions:
  name: "hallway-screen"
  friendly_name: "Hallway Screen"

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

packages:
  setup:
    url: https://github.com/jtenniswood/espcontrol/
    file: devices/esp32-p4-wifi6-touch-lcd-5/packages.yaml
    refresh: 1sec
```

After flashing, check boot, Wi-Fi setup, Home Assistant pairing, touch alignment in all four rotations, brightness, the speaker, and a firmware update before relying on the panel.
