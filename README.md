#**ESP32-CAM Streaming with GStreamer & DeepStream**

**Overview**

This repository provides modified versions of the DeepStream Test1 and Test3 applications to support low-cost ESP32-CAM streaming. If you're looking to integrate ESP32-CAM with NVIDIA DeepStream, these modifications help handle its MJPEG compression, which differs from the H.264 format used in standard DeepStream pipelines.

Why Use ESP32-CAM?

The ESP32-CAM is an affordable, Wi-Fi-enabled camera module ideal for streaming applications. However, it natively encodes video in MJPEG format, while DeepStream expects H.264. This repository bridges that gap by providing custom GStreamer pipelines to decode the MJPEG stream and integrate it into DeepStream.

**How It Works**

Run the ESP32-CAM RTSP server: First, upload an Arduino sketch to the ESP32-CAM to stream over RTSP in MJPEG format.

Ensure DeepStream is installed: Before modifying anything, make sure that DeepStream's default applications run correctly.

Replace DeepStream Test1 or Test3 application:

For a single camera stream, replace deepstream_test1_app.c.

For multiple camera streams, replace deepstream_test3_app.c.

Compile & run the modified application to handle MJPEG streaming from ESP32-CAM.

**Key Features**

Supports MJPEG decoding for ESP32-CAM streams.

Uses GStreamer to handle RTSP input and integrate it into DeepStream.

Modifies DeepStream Test1 application to allow a single RTSP camera stream.

Modifies DeepStream Test3 application to handle multiple RTSP camera streams.

Requires minimal setup beyond DeepStream dependencies.

**Prerequisites**

Before using this, ensure that:

DeepStream is installed and its sample applications run correctly.

ESP32-CAM RTSP streaming is working (via Arduino sketch).

You have installed the same dependencies required by DeepStream.

**Installation & Usage**

1. Set Up ESP32-CAM for RTSP Streaming

Upload an Arduino sketch that:

Initializes the camera.

Streams video over RTSP using MJPEG compression.

2. Install Required Dependencies

Ensure you have GStreamer and DeepStream dependencies installed:

sudo apt update && sudo apt install -y gstreamer1.0-tools gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly

3. Replace DeepStream Test1 or Test3 Code

For single camera streaming: Copy deepstream_test1_app.c into:

/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test1/

For multiple camera streaming: Copy deepstream_test3_app.c into:

/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test3/

Then compile and run the application.

**Notes**

The Test1 modification supports only one RTSP camera at a time.

The Test3 modification allows multiple RTSP camera streams.

Ensure that the original DeepStream applications run successfully before using these modified versions.

DeepStream 7.1 introduces .yml configuration files for deep learning pipelines, but this project does not modify the AI-related parts—only the GStreamer pipeline.

**License**

This project is open-source and provided as-is. Feel free to modify and improve it!

