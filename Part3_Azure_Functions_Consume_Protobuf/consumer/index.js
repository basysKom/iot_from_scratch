/*
* Copyright (c) 2020 basysKom GmbH
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

const messages = require('../generated/environment_iot_messages_pb.js')

module.exports = function (context, IoTHubMessages) {

    context.bindings.sensorDataTableBinding = []
    context.bindings.eventsTableBinding = []

    for (let i = 0; i < IoTHubMessages.length; ++i) {
        const deserializedMessage = new proto.iotexample.DeviceMessages.deserializeBinary(IoTHubMessages[i]);

        const sender = context.bindingData.systemPropertiesArray[i]['iothub-connection-device-id'];

        for (const message of deserializedMessage.getTelemetryMessagesList()) {
            console.log(`Message from device ${sender}`);

            let data = message.toObject();

            const tableStorageEntry = {
                PartitionKey: sender,
                RowKey: data.timestamp,
                sourceTimestamp: new Date(data.timestamp)
            }

            if (data.environmentData) {
                tableStorageEntry.PartitionKey += '_environmentData';
                context.bindings.sensorDataTableBinding.push({...tableStorageEntry, ...data.environmentData});
            } else if (data.event) {
                context.bindings.eventsTableBinding.push({...tableStorageEntry, ...data.event});
            }
        }
    }

    context.done();
};
