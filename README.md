# Smart Doorbell

A portable smart doorbell system built using the **Seeed Studio XIAO ESP32-S3 Sense**, a PIR motion sensor, physical doorbell button, and a Flask web application.

The system is designed to detect motion or a doorbell button press, capture an image, and send the event over Wi-Fi to a Flask server running on a computer. The server will store the event and display it through a web dashboard.

> **Status:** 🚧 Currently in development

---

## Project Goal

The goal of this project is to build a complete embedded system that combines hardware, firmware, networking, backend development, and a web interface.

The first version will support:

- PIR motion detection
- Physical doorbell button detection
- Automatic image capture
- Wi-Fi communication
- Image upload to a computer
- Event storage
- Event timestamps
- Web-based event history

---

## System Architecture

```text
PIR Motion Sensor ─────┐
                       │
Doorbell Button ───────┤
                       ↓
              XIAO ESP32-S3 Sense
                       │
                    Camera
                       │
                     Wi-Fi
                       │
                       ↓
                Flask REST API
                       │
              ┌────────┴────────┐
              ↓                 ↓
           SQLite            Images
              │                 │
              └────────┬────────┘
                       ↓
                 Web Dashboard
