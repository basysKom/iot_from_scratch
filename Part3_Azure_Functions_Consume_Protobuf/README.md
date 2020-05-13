# Azure Function to consume Protobuf messages from IoT Hub

This Azure Function App uses an EventHub trigger to consume Protobuf messages from IoT Hub.

The messages are deserialized and the content is inserted into Azure Storage tables.
