#!/bin/bash
# Push project to GitHub
# Prerequisite: Create repo at https://github.com/Nealcn/yuyin first

git init
git remote add origin https://github.com/Nealcn/yuyin.git
git add -A
git commit -m "Initial commit: M5AtomS3R Voice Stick

- NimBLE BLE stack (advertising as VS-XXXX)
- PDM microphone driver (16kHz/16bit I2S PDM)
- Opus audio encoder
- Single button (hold-to-talk, double-click cancel)
- LCD display driver placeholder
- Windows desktop client (voicestick compatible)
- BLE protocol compatible with 78/voicestick"
git push -u origin master
