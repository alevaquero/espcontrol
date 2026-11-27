---
title: "Home Assistant Remote Cards"
description:
  How to use Remote cards on your EspControl panel to control an Apple TV, Android TV, Roku, or other Home Assistant remote entity.
---

# Remote

A Remote card turns the panel into a TV remote. The card shows the remote's name and lights up while the remote entity is on. Tap it to open a remote with a direction pad, volume keys, and a row of extra buttons.

Each button sends a command to a Home Assistant `remote` entity using the `remote.send_command` action. Add your Apple TV, Android TV, Roku, or other remote integration in Home Assistant first.

## Setting Up a Remote Card

1. Select a card and change its type to **Remote**.
2. Choose the **Remote Entity**, for example `remote.living_room`.
3. Optionally set a **Label** and **Icon**. Without a label, the card uses the entity name.
4. Open **Remote Buttons** and check the command for each button.

## Remote Buttons

New Remote cards start with Apple TV commands. Change any command to match your remote, or leave a button empty to hide it.

| Button | Apple TV command |
|---|---|
| Up, Down, Left, Right | `up`, `down`, `left`, `right` |
| OK | `select` |
| Back | `menu` |
| Home | `home` |
| Volume Up, Volume Down | `volume_up`, `volume_down` |
| Power | `suspend` (sleep) |

**Menu**, **Mute**, **Rewind**, **Play/Pause**, and **Fast Forward** start empty. Apple TV has no mute command; for the others you can use `top_menu`, `skip_backward`, `play_pause`, and `skip_forward`.

The panel sends each command exactly as typed and does not check whether your remote supports it. If a button does nothing, check the command names listed in your remote integration's Home Assistant documentation.

Use the arrows next to the **Button Row** entries to change the order of the extra buttons.

### Touchpad Navigation

Set **Remote Buttons > Navigation** to **Touchpad** to replace the arrow keys with one touch surface, like the Apple TV remote on a phone:

- **Swipe** to move. A short flick sends one Up, Down, Left, or Right command; a longer slide sends one more for each fifth of the touchpad it travels, so you can scroll through a list in one movement. Changing direction mid-slide follows your finger.
- **Tap** to send OK. It is sent a fifth of a second after you lift your finger, so the first touch of a swipe is never taken as a tap. Holding your finger still does nothing.

The touchpad uses the same Up, Down, Left, Right, and OK commands as the arrows. Leave one empty and that gesture does nothing. **Arrows** is the default.

### Other Remotes

| Remote | Example commands |
|---|---|
| Android TV Remote | `DPAD_UP`, `DPAD_CENTER`, `BACK`, `HOME`, `VOLUME_UP`, `VOLUME_MUTE`, `MEDIA_PLAY_PAUSE` |
| Roku | `up`, `select`, `back`, `home`, `volume_up`, `volume_mute`, `play` |

## Power

The **Power** button sends a single command. On Apple TV, `suspend` puts the box to sleep, and pressing any other button, such as **Home**, wakes it again.
