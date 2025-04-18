# Nomo

[![Website](https://img.shields.io/badge/Website-nomo--capstone.framer.website-blue)](https://nomo-capstone.framer.website/)
![Last Commit](https://img.shields.io/github/last-commit/sripranav9/nomo)

**A Little Friend for a Big Focus**

Nomo is an interactive desk companion robot built to encourage healthier work habits through computer vision, gesture recognition, and playful nudges. Designed as a capstone project at NYU Abu Dhabi, Nomo blends real-time software with expressive hardware to promote mindful breaks and deep focus—making your workflow a little more human.

## 🏷️ Topics
`interactive-media` `robotics` `robot-companion` `machine-learning` `google-mediapipe` `raspberry-pi-5` `arduino` `wellbeing` `capstone-project` `pomodoro`


## 🔧 Features

- Real-time gesture recognition (wave, closed fist, thumbs-up)
- LED feedback to guide breaks and focus sessions
- Raspberry Pi + Arduino integration
- Pettable with responsive purring vibration
- Location-based behavior (work and break zones)
- Designed with routines, attention, and well-being in mind

## 🧠 Built With

- [MediaPipe](https://github.com/google/mediapipe) – real-time hand tracking and gesture recognition
- [OpenCV](https://opencv.org/) – image capture, annotation, and video streaming
- [Flask](https://flask.palletsprojects.com/) – lightweight web framework for video streaming
- [Picamera2](https://github.com/raspberrypi/picamera2) – camera interface for Raspberry Pi
- [Arduino](https://www.arduino.cc/) – microcontroller used for motion, sound, and feedback
- [Python](https://www.python.org/) – control logic, CV processing, and server orchestration
- [Pyserial](https://pypi.org/project/pyserial/) – serial communication for Pi-to-Arduino command flow
- [Raspberry Pi 5](https://www.raspberrypi.com/products/raspberry-pi-5/) – main compute unit for processing gestures, state transitions, and control logic


## 🚀 How It Works

Nomo runs on a Raspberry Pi using Python for vision and logic, paired with an Arduino for motor and sensor control. When placed in a predefined location, it can detect gestures and trigger break/focus states accordingly. Each mode is designed to reflect a balance between productivity and self-care.

> For a full overview of modes, gestures, and setup, see the [Product Manual](/docs/nomo-product-manual.pdf).


## 📁 Repository Structure

```bash
nomo/
├── pi/                 # Raspberry Pi scripts (python - gesture, logic)
├── arduino/            # Arduino sketches (servo, vibration, LEDs, sounds)
├── assets/             # Gesture samples, hero images, mockups
├── docs/               # Product manual
└── README.md

```

## 🔒 Project Use Notice

This was developed as part of a Capstone Project at **NYU Abu Dhabi** (2025).

The code and associated materials are made available under the terms of the [MIT License](LICENSE).  
They may be freely used, modified, and distributed for personal, academic, and non-commercial purposes.

**Commercial use, resale, or distribution of derivative works for profit is not permitted without prior written consent of both project authors.**

