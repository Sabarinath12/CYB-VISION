
# CYB-ViSION
A high-performance, T-800-inspired HUD-based surveillance interface in C++


![image](https://github.com/user-attachments/assets/9a400aee-ea23-4367-85f1-99f5b84de020)



## Overview

CYB-ViSION is a cutting-edge computer vision application that provides real-time face detection and surveillance capabilities with a cybernetic heads-up display (HUD) inspired by the T-800 terminator. This system combines high-performance image processing with an immersive visual interface that displays system metrics, kernel logs, and target information.
Features

Real-time Face Detection: Identifies human subjects using OpenCV's Haar cascade classifier
Automated Image Capture: Takes snapshots when faces are detected and stores them with timestamps
Dynamic HUD Interface:

Cybernetic visual overlay with color-coded status indicators
Real-time system metrics (CPU, RAM, Storage, Network)
Live kernel log display with severity-based color coding
Dynamic tinting based on operational state


System Monitoring:

CPU and RAM usage tracking
Storage capacity monitoring
Network connectivity status
Battery level reporting (when available)


Intelligent Subject Tracking:

Distinguishes between new and previously detected subjects
Cooldown period between captures to prevent redundant images
Analysis state visualization after subject capture


Resource Management:

Memory usage optimization
Frame rate control for consistent performance
Efficient thread management


## Requirements

* OpenCV 4.x or higher
* C++17 compatible compiler
* Linux-based system (for /proc filesystem access)
* V4L2-compatible webcam
* CMake 3.10+ (for building)


## Build & Run

make && ./main

## Usage
#### Upon launching, CYB-ViSION

* Automatically initialize the camera and face detection systems
* Display a real-time video feed with cybernetic HUD overlay
* Monitor for human subjects in the camera's field of view
* Capture images of detected faces and store them in the snapshot directory
* Switch to "analysis mode" after capturing an image, with distinct visual indicators
* Return to monitoring mode after a cooldown period
## Technical Details

### Core Components

* Face Detection: Utilizes Haar cascade classifiers for efficient face recognition
* Multithreading: Separates system monitoring, network checks, and UI rendering
* Kernel Log Simulation: Generates plausible system messages based on current state
* Resource Monitoring: Tracks system metrics via /proc filesystem
* Adaptive Frame Processing: Adjusts processing based on available system resources

### Visual Interface

* Blue Tint: Indicates standard monitoring mode
* Red Tint: Activates during analysis mode after image capture
* Dynamic Logs: Shows context-appropriate messages based on detection status
* Status Metrics: Displays system performance in real-time## Usage
#### Upon launching, CYB-ViSION

* Automatically initialize the camera and face detection systems
* Display a real-time video feed with cybernetic HUD overlay
* Monitor for human subjects in the camera's field of view
* Capture images of detected faces and store them in the snapshot directory
* Switch to "analysis mode" after capturing an image, with distinct visual indicators
* Return to monitoring mode after a cooldown period



Contributions are welcome! Please feel free to submit a Pull Request.
