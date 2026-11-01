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

Open **Right Sensor** to show a second door or window on the same card. The card splits in two: the first sensor keeps the icon and label on the left, and the second sensor shows its own icon and label on the right.

1. Choose the **Right Type**, **Door** or **Window**, for the second sensor's icons.
2. Enter the **Right Sensor Entity**, for example `binary_sensor.kitchen_window`.
3. Set a **Right Label** if you want custom text. If left blank, the panel uses the entity name from Home Assistant.
4. **Right Lit When Open** works like **Lit When Open**, but only for the right half. It is on by default when you add the second sensor.

Each half lights up on its own, so if one is open and the other is closed, only the open half uses the active colour. The card shows as unavailable only when both sensors are unavailable. Clear the **Right Sensor Entity** to return to a single-sensor card.

## How It Works on the Panel

- The card is read-only, so tapping it does nothing.
- The icon changes when Home Assistant reports an active state such as `on`, `open`, `opening`, `closing`, or `unlocked`.
- By default, **Lit When Open** is on, so the card uses the normal active/on colour when open. Turn it off if you only want the icon to change.
- When the sensor is off, closed, unknown, or unavailable, the card returns to the Sensor card colour and closed icon.
