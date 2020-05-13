# IoT: Getting started with cloud and modern IoT and IIoT from scratch

IoT and IIoT applications are special compared to other kinds of cloud applications as they have to deal with devices existing “outside” of data centers.

These devices often have only restricted network connectivity (low bandwidth, battery constraints or being online only for a limited amount of time). In addition, they need to securely reach their service endpoints in the cloud via untrusty or outright malicious networks.

Another challenge is that these devices are possibly in the hands of end users with limited technical abilities or in the hands of attackers manipulating the device and its associated services.

The mentioned issues have lead major cloud providers to offer specialized services targeting IoT and IIoT scenarios to ease implementation.

## IoT from scratch
In the next days we will release the following series of articles to provide you an end-to-end overview of what Microsoft Azure offers to handle those challenges. 

### Azure IoT Hub: Connecting a Qt Application with Azure (Part 1 of 4)
What is the IoT Hub and what problems does it solve?
Example: Basic usage of the Microsoft Azure IoT SDK

### Protobuf for IoT: Sending cost-optimized into the cloud (Part 2 of 4)
What is Protobuf and how does it fit into a device-to-cloud / cloud-to-device scenario?
Example: Sending of Protobuf encoded data via the Microsoft Azure IoT SDK (C++ and Javascript)

### How to consume Protobuf messages in Azure Functions (Part 3 of 4)
An introduction to the concept of Microsoft Azure Functions.
Example: Decoding and further processing of Protobuf encoded data from an IoT Hub via Microsoft Azure Functions

### IoT Hub Device Provisioning Service, bring your IoT Devices into the Cloud. (Part 4 of 4)
How does the Device Provisioning Service provide a way to securely link a device and an IoT Hub?
Example: Usage of the DPS together with a device-side secure element (TPM) via Microsoft Azure IoT SDK (C++)

By working through this series you will learn about the major concepts involved in getting your IoT/IIoT device connected to Microsoft Azure.
The resulting working prototype enables you to make your own first steps. Turning this initial prototype into a product involves a bit more work and experience though. You will need to handle issues of scalability, automatic service deployments, continuous integration, system integration for Embedded Linux and more. 

We are interested to [hear|mailto://sales@basyskom.com] about your upcoming IoT/IIoT projects and would be glad to help. 

Checkout our blog at to learn more: https://blog.basyskom.com/2020/cloud-and-iot-from-scratch/
