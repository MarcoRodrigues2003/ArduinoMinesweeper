# Arduino Minesweeper 

A physical prototype of the classic **Minesweeper** built on **Arduino** using an **8×8 WS2812B LED matrix** (64 LEDs) as the display and **four CD74HC4067 16-channel multiplexers** to read a grid of inputs.

This project focuses on:
- Reliable **multiplexed input scanning**
- Clear **LED feedback** for Minesweeper numbers
- A simple “**one touch = one action**” interaction model (with input locking)
- Basic win/lose effects and automatic restart

---

## Features

- **8×8 grid** (64 cells) mapped directly to WS2812B LEDs
- **10 mines** randomly placed each game (configurable)
- Neighbor mine counts (0–8) with different colors
- **Flood fill reveal** for empty (0) cells
- **Stable button/touch read** via mux sampling + settle/flush
- **Input lock**: prevents repeated actions while holding a cell
- End game feedback:
  - **Green blink** = win
  - **Red blink** = loss
  - Restarts automatically after ~5 seconds

---

## Hardware

### Required
- Arduino (tested with typical UNO/Nano-style setup)
- WS2812B LED matrix **8×8** (64 LEDs)
- **4× CD74HC4067** (16-channel mux each) to cover 64 inputs
- Input mechanism per cell (buttons / foil contacts / touch pads, etc.)
- 5V power (WS2812B may require an external 5V supply depending on brightness/current)

### Libraries
- **FastLED**
- **CD74HC4067** library

---

## Wiring (as used in the sketch)

### WS2812B Matrix
- `LED_PIN` → **D10**
- `NUM_LEDS` → **64**
- `BRIGHTNESS` → **15** (safe default; adjust carefully)

### CD74HC4067 Address Pins (shared across all 4 mux)
- `S0` → **D2**
- `S1` → **D3**
- `S2` → **D4**
- `S3` → **D5**

### Mux Signal Pins (one per mux)
Each mux output (SIG) is connected to a different analog pin:
- MUX 0 `SIG` → **A0**
- MUX 1 `SIG` → **A1**
- MUX 2 `SIG` → **A2**
- MUX 3 `SIG` → **A3**

Inputs are read using:
- `pinMode(SIG, INPUT_PULLUP)`
- A press/touch is detected when the signal reads **LOW**

> Important: make sure your input hardware pulls the SIG line to **GND** when “pressed”.

---

## How the grid is mapped

- The board is indexed as `idx = y * 8 + x`
- `idx 0..15` → MUX 0 channels `0..15`
- `idx 16..31` → MUX 1 channels `0..15`
- `idx 32..47` → MUX 2 channels `0..15`
- `idx 48..63` → MUX 3 channels `0..15`

LED mapping:
- Default uses a **non-serpentine** layout:
  - `SERPENTINE_LAYOUT = false`
- If your LED matrix is wired serpentine, set:
  - `SERPENTINE_LAYOUT = true`

---

## Gameplay

- Touch/press a cell to reveal it.
- If it contains a mine: **LOSE** (red blink).
- If it is safe:
  - Shows a color based on neighbor mine count (0–8).
  - If count is 0, flood-fill reveals adjacent empty cells.
- When all safe cells are revealed: **WIN** (green blink).
- After the end animation (~5 seconds), a new game starts automatically.

---
