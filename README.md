# Nomo

**A Little Friend for a Big Focus**

Nomo is an interactive desk companion robot built to encourage healthier work habits through computer vision, gesture recognition, and playful nudges. Designed as a capstone project at NYU Abu Dhabi, Nomo blends real-time software with expressive hardware to promote mindful breaks and deep focus—making your workflow a little more human.

---

## 🔧 Features

- 🧠 Real-time gesture recognition (wave, closed fist, thumbs-up)
- 💡 LED feedback to guide breaks and focus sessions
- 🤖 Raspberry Pi + Arduino integration
- 🐾 Pettable with responsive purring vibration
- 📍 Location-based behavior (work and break zones)
- 🎯 Designed with routines, attention, and well-being in mind

---

## 🚀 How It Works

Nomo runs on a Raspberry Pi using Python for vision and logic, paired with an Arduino for motor and sensor control. When placed in a predefined location, it can detect gestures and trigger break/focus states accordingly. Each mode is designed to reflect a balance between productivity and self-care.

> For a full overview of modes, gestures, and setup, see the [Product Manual](link).

---

## 📁 Repository Structure

```bash
nomo/
├── pi/                 # Raspberry Pi scripts (Python - gesture, logic)
├── arduino/            # Arduino sketches (servo, vibration, LEDs)
├── assets/             # Media, logos, mockups
├── docs/               # Product manual, slides, website content
├── web/                # Framer site export or related media
└── README.md

