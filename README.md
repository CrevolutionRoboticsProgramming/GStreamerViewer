# GStreamerViewer

### Crevolution's client-side GStreamer application for receiving an MJPG stream via UDP from our [OffseasonVision2019](https://github.com/CrevolutionRoboticsProgramming/OffseasonVision2019) vision processing program

## Purpose

This application receives the video stream sent by our [OffseasonVision2019](https://github.com/CrevolutionRoboticsProgramming/OffseasonVision2019) vision processing program, draws a line down the middle, and displays it in a 640x480 window.

## Usage

Download the repo as a .zip, extract the files, extract the .dll files in x64/Release, and run the executable in x64/Release. No window will appear until a valid video stream is identified. The only video stream accepted by the application is an MJPG stream sent by GStreamer via UDP to port 1181. The format for creating this video stream can be found in [this repo](https://github.com/CrevolutionRoboticsProgramming/OffseasonVision2019).
