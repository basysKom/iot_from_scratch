# Qt based IoT Hub publisher with Protobuf and Device Provisioning Service support

This Qt based application demonstrates basic interaction with Azure IoT Hub and Device Provisioning Service
- Retrieving provisioning information from Device Provisioning Service using a TPM
- Reacting to device twin changes and updating the device twin
- Sending Protobuf messages to IoT Hub

## Azure IoT SDK

This application requires https://github.com/Azure/azure-iot-sdk-c

The .pro file is intended for a static build with provisioning client and TPM simulator enabled.

https://github.com/protocolbuffers/protobuf is required for message decoding.

The Protobuf compiler is used by qmake to generate header and source files for the
installed protobuf version.
