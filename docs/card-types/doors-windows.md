---
title: "Home Assistant Doors & Windows Cards"
description:
  How to show Home Assistant door and window sensors with open and closed icons on your EspControl panel.
---

# Doors & Windows

A Doors & Windows card is a read-only card for contact sensors. It uses the Sensor card style, but picks suitable icons for open and closed states.

## Setting Up a Doors & Windows Card

1. Select a card and change its type to **Doors & Windows**.
2. Choose **Door** or **Window**.
3. Enter the **Sensor Entity** for the contact sensor, for example `binary_sensor.patio_door`.
4. Set a **Label** if you want custom text. If left blank, the panel uses the entity name from Home Assistant when it can.

The default icon pairs are:

| Subtype | Closed icon | Open icon |
|---|---|---|
| Door | Door | Door Open |
| Window | Window Closed | Window Open |

You can change the closed or open icon if your sensor needs a different look.

## Two Sensors on One Card

Use **Show** at the top of the card settings to choose which halves of the card have a sensor:

- **Left**: a normal single-sensor card.
- **Right**: one sensor shown on the right half, with the left half left empty.
- **Both**: two sensors side by side, each on its own half.

The settings are split into **Left Card Settings** and **Right Card Settings**, and only the sections for the halves you show are visible. Each section has the same options:

1. **Type**: **Door** or **Window**, which picks that side's default icons.
2. **Sensor Entity**: the contact sensor, for example `binary_sensor.kitchen_window`.
3. **Label**: custom text. If left blank, the panel uses the entity name from Home Assistant.
4. **Closed Icon** and **Open Icon**: change these if that sensor needs a different look.
5. **Lit When Open**: lights only that half of the card when its sensor is open. It is on by default when you add a right sensor.

Each half lights up on its own, so if one is open and the other is closed, only the open half uses the active colour. With both sides shown, the card shows as unavailable only when both sensors are unavailable. Switching **Show** back to **Left** returns to a single-sensor card.

## How It Works on the Panel

- The card is read-only, so tapping it does nothing.
- The icon changes when Home Assistant reports an active state such as `on`, `open`, `opening`, `closing`, or `unlocked`.
- By default, **Lit When Open** is on, so the card uses the normal active/on colour when open. Turn it off if you only want the icon to change.
- When the sensor is off, closed, unknown, or unavailable, the card returns to the Sensor card colour and closed icon.
