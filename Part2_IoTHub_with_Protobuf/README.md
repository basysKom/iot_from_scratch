# Qt based IoT Hub publisher with Protobuf

This Qt based application demonstrates basic interaction with Azure IoT Hub
- Reacting to device twin changes and updating the device twin
- Sending Protobuf messages to IoT Hub

## Azure IoT SDK

This application requires https://github.com/Azure/azure-iot-sdk-c

The .pro file is intended for a static build without provisioning client enabled.

https://github.com/protocolbuffers/protobuf is required for message decoding.

The Protobuf compiler is used by qmake to generate header and source files for the
installed protobuf version.
