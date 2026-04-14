# Scroll Pedal — Wireless BLE Foot Controller for Musicians

A wireless BLE foot pedal for hands-free chord sheet scrolling. Built on ESP32 with ESP-IDF — advertises as a native HID keyboard, no drivers or app needed. Single press scrolls down, double-tap scrolls up, hold to power off.


I have a terrible memory for chord charts. Flipping between a capo'd shape and whatever comes next always means stopping to scroll my screen mid-song — which breaks the flow completely. So I built my own solution: a wireless foot pedal that lets me scroll chord sheets hands-free while playing.

The pedal is a custom ESP32-based device running firmware built on ESP-IDF and FreeRTOS. It advertises itself over BLE as a standard HID keyboard (report descriptor included), so it pairs natively with any phone, tablet, or laptop — no app or driver needed. A single foot press sends a Down Arrow keycode; a quick double-tap sends Up Arrow; holding it triggers a clean power-off sequence.

Under the hood:

BLE advertisement & GATT stack — the device advertises with ESP_HID_APPEARANCE_KEYBOARD, registers a GATTS callback, and initializes a HIDD device via esp_hidd_dev_init. On disconnect it automatically re-advertises.
HID Report Descriptor — an 8-byte keyboard report (Report ID 1) with a standard modifier/reserved/keycode layout, conforming to the USB HID spec so the host OS treats it as a real keyboard without any pairing quirks.
Interrupt-driven button driver — GPIO ISR with 20 ms debounce, FreeRTOS task notification wakeup, long-press detection, and a multi-callback registration system.
Power management — a power-latch circuit keeps the MCU alive after button release; deep sleep is entered via esp_deep_sleep_start() with GPIO wakeup configured, so the device wakes only on a deliberate button hold.
Double-press detection — a 400 ms window in the button callback distinguishes single vs. double press without a secondary timer task.
The result is a device that the OS sees as a keyboard, costs almost nothing to build, and pairs in seconds — so I can focus on playing instead of scrolling.

