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

#include <generated/environment_iot_messages.pb.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTimer>

#include <azureiot/iothub.h>
#include <azureiot/iothub_device_client_ll.h>
#include <azureiot/iothub_client_options.h>
#include <azureiot/iothub_message.h>
#include <azureiot/iothubtransportamqp.h>

#include "azure_prov_client/prov_device_ll_client.h"
#include <azure_prov_client/prov_transport_amqp_client.h>
#include <azure_prov_client/prov_security_factory.h>
#include <azure_prov_client/prov_auth_client.h>

class IotHubClient : public QObject
{
    Q_OBJECT

public:
    IotHubClient(QObject *parent = nullptr);
    ~IotHubClient();

    bool init(const QString &idScope);

    bool sendMessage(int id, const QByteArray &data);
    bool updateDeviceTwin(const QJsonObject &reported);

    bool connected() const;

    bool queryTpmInformation();

signals:
    void connectedChanged(bool connected);
    void deviceTwinUpdated(int statusCode);
    void messageStatusChanged(int id, bool success);
    void desiredObjectChanged(QJsonObject desired);

private:
    // Callbacks for the DPS client
    static void registerDeviceCallback(PROV_DEVICE_RESULT result, const char* iotHubUri,
                                  const char* deviceId, void* context);
    static void registrationStatusCallback(PROV_DEVICE_REG_STATUS registrationStatus, void* context);

    // Callbacks for the IoT Hub client
    static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                         IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
                                         void* context);
    static void sendConfirmCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* context);
    static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char* payload,
                                   size_t size, void* context);
    static void reportedStateCallback(int statusCode, void* context);

    // Call the IOT SDK's DoWork functions
    void doWork();

    bool requestProvisioningData();
    void initializeIotHub(const QString &iotHubUri, const QString &deviceId);

    void setConnected(bool connected);

    struct MessageContext {
        MessageContext(IotHubClient *client, int id, IOTHUB_MESSAGE_HANDLE message) {
            this->client = client;
            this->id = id;
            this->message = message;
        }
        IotHubClient *client;
        int id;
        IOTHUB_MESSAGE_HANDLE message;
    };

    QTimer mDoWorkTimer;
    IOTHUB_DEVICE_CLIENT_LL_HANDLE mDeviceHandle = nullptr;
    PROV_DEVICE_LL_HANDLE mProvisioningHandle = nullptr;
    QString mIdScope;
    bool mConnected = false;
};

IotHubClient::IotHubClient(QObject *parent)
    : QObject(parent)
{
    QObject::connect(&mDoWorkTimer, &QTimer::timeout, this, &IotHubClient::doWork);
    mDoWorkTimer.setInterval(200);
}

IotHubClient::~IotHubClient()
{
    mDoWorkTimer.stop();
    if (mDeviceHandle)
        IoTHubDeviceClient_LL_Destroy(mDeviceHandle);
    IoTHub_Deinit();

    if (mProvisioningHandle)
        Prov_Device_LL_Destroy(mProvisioningHandle);
    prov_dev_security_deinit();
}

void IotHubClient::doWork()
{
    if (mDeviceHandle)
        IoTHubDeviceClient_LL_DoWork(mDeviceHandle);
    if (mProvisioningHandle)
        Prov_Device_LL_DoWork(mProvisioningHandle);
}

bool IotHubClient::requestProvisioningData()
{
    if (mProvisioningHandle) {
        Prov_Device_LL_Destroy(mProvisioningHandle);
        mProvisioningHandle = nullptr;
    }

    mProvisioningHandle = Prov_Device_LL_Create("global.azure-devices-provisioning.net",
                                                mIdScope.toLatin1().constData(),
                                                Prov_Device_AMQP_Protocol);

    if (!mProvisioningHandle) {
        qWarning() << "Failed to create DPS client";
        return false;
    }

    auto result = Prov_Device_LL_Register_Device(mProvisioningHandle, registerDeviceCallback, this,
                                                 registrationStatusCallback, this);

    if (result != PROV_DEVICE_RESULT_OK) {
        qWarning() << "Failed to dispatch register device with result" << result;
        return false;
    }

    return true;
}

void IotHubClient::initializeIotHub(const QString &iotHubUri, const QString &deviceId)
{
    // Destroy the provisioning client to unblock TPM access
    if (mProvisioningHandle) {
        Prov_Device_LL_Destroy(mProvisioningHandle);
        mProvisioningHandle = nullptr;

        prov_dev_security_deinit();
    }

    // Destroy previous client
    if (mDeviceHandle) {
        IoTHubDeviceClient_LL_Destroy(mDeviceHandle);
        mDeviceHandle = nullptr;
    }

    mDeviceHandle = IoTHubDeviceClient_LL_CreateFromDeviceAuth(iotHubUri.toUtf8().constData(),
                                                               deviceId.toUtf8().constData(),
                                                               AMQP_Protocol);

    if (!mDeviceHandle) {
        qWarning() << "Failed to create client from connection string";
        return;
    }

    qDebug() << "Client created";

    auto result = IoTHubDeviceClient_LL_SetConnectionStatusCallback(mDeviceHandle,
                                                                    connectionStatusCallback,
                                                                    this);

    if (result != IOTHUB_CLIENT_OK) {
        qWarning() << "Failed to set connection status callback with result" << result;
        return;
    }

    result = IoTHubDeviceClient_LL_SetDeviceTwinCallback(mDeviceHandle, deviceTwinCallback, this);

    if (result != IOTHUB_CLIENT_OK) {
        qWarning() << "Failed to set device twin callback with result" << result;
        return;
    }
}

bool IotHubClient::connected() const
{
    return mConnected;
}

bool IotHubClient::queryTpmInformation()
{
    PROV_AUTH_HANDLE authHandle = prov_auth_create();
    if (!authHandle) {
        qDebug() << "Provisioning authentication handle creation failed";
        return false;
    }

    QByteArray endorsementKey;
    QString registrationId;
    char *registrationIdBuffer = nullptr;

    BUFFER_HANDLE endorsementKeyBuffer = prov_auth_get_endorsement_key(authHandle);

    if (!endorsementKeyBuffer) {
        qDebug() << "Failed to query endorsement key";
        prov_auth_destroy(authHandle);
        return false;
    } else {
        endorsementKey = QByteArray(reinterpret_cast<const char *>(BUFFER_u_char(endorsementKeyBuffer)),
                        BUFFER_length(endorsementKeyBuffer));
        BUFFER_delete(endorsementKeyBuffer);
    }

    registrationIdBuffer = prov_auth_get_registration_id(authHandle);
    if (!endorsementKeyBuffer) {
        qDebug() << "Failed to query registration id";
        prov_auth_destroy(authHandle);
        return false;
    } else {
        registrationId = QString::fromLatin1(registrationIdBuffer);
        free(registrationIdBuffer);
    }

    prov_auth_destroy(authHandle);

    qDebug() << "Registration id:" << registrationId;
    qDebug() << "Endorsement key:" << endorsementKey.toBase64();

    return true;
}

void IotHubClient::registerDeviceCallback(PROV_DEVICE_RESULT result, const char *iotHubUri,
                                          const char *deviceId, void *context)
{
    qDebug() << "Register device callback with result" << result;

    auto client = static_cast<IotHubClient *>(context);

    if (result == PROV_DEVICE_RESULT_OK) {
        qDebug() << "Provisioning information received";
        qDebug() << "IoT Hub:" << iotHubUri << "Device id:" << deviceId;

        const auto uri = QString::fromUtf8(iotHubUri);
        const auto id = QString::fromUtf8(deviceId);

        QTimer::singleShot(1000, client, [=]() {
            client->initializeIotHub(uri, id);
        });
    } else {
        qDebug() << "Provisioning failed, retry";
        QTimer::singleShot(5000, client, [=]() {
            client->requestProvisioningData();
        });
    }
}

void IotHubClient::registrationStatusCallback(PROV_DEVICE_REG_STATUS registrationStatus, void *context)
{
    Q_UNUSED(context);
    qDebug() << "Registration status callback with status" << registrationStatus;
}

void IotHubClient::setConnected(bool connected)
{
    if (mConnected != connected) {
        mConnected = connected;
        emit connectedChanged(mConnected);
    }
}

bool IotHubClient::init(const QString &idScope)
{
    if (mProvisioningHandle || mDeviceHandle) {
        qDebug() << "Client is already initialized";
        return false;
    }

    mIdScope = idScope;

    auto result = IoTHub_Init();

    if (result != IOTHUB_CLIENT_OK) {
        qWarning() << "IoTHub_Init failed with result" << result;
        return false;
    }

    result = prov_dev_security_init(SECURE_DEVICE_TYPE_TPM);

    if (result != IOTHUB_CLIENT_OK) {
        qWarning() << "prov_dev_security_init failed with result" << result;
        return false;
    }

    queryTpmInformation();

    const auto success = requestProvisioningData();

    if (success)
        mDoWorkTimer.start();

    return success;
}

void IotHubClient::connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                            IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
                                            void *context)
{
    qDebug() << "Connection status changed to" << MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS, result) <<
                "with reason" << MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONNECTION_STATUS_REASON, reason);

    const auto client = static_cast<IotHubClient *>(context);

    client->setConnected(result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED);
}

void IotHubClient::sendConfirmCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *context)
{
    const auto messageContext = static_cast<IotHubClient::MessageContext *>(context);

    qDebug() << "Send confirm callback for message" << messageContext->id << "with" <<
                MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result);

    emit messageContext->client->messageStatusChanged(messageContext->id, result == IOTHUB_CLIENT_CONFIRMATION_OK);

    IoTHubMessage_Destroy(messageContext->message);
    delete messageContext;
}

void IotHubClient::deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payload,
                                      size_t size, void *context)
{
    qDebug() << "Received" << (updateState == DEVICE_TWIN_UPDATE_PARTIAL ? "partial" : "full") << "device twin update";

    const auto client = static_cast<IotHubClient *>(context);

    const auto data = QByteArray::fromRawData(reinterpret_cast<const char *>(payload), size);

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error) {
        qWarning() << "Failed to parse JSON document:" << parseError.errorString();
        return;
    }

    if (!document.isObject()) {
        qWarning() << "JSON document is not an object";
        return;
    }

    const auto deviceTwinObject = document.object();
    qDebug() << "Device twin content:" << deviceTwinObject;

    auto desired = deviceTwinObject;

    // Partial updates contain the desired object without an enclosing object
    if (updateState == DEVICE_TWIN_UPDATE_COMPLETE) {
        const auto jsonIterator = deviceTwinObject.constFind(QLatin1String("desired"));
        if (jsonIterator == deviceTwinObject.constEnd() || !jsonIterator->isObject()) {
            qWarning() << "The desired property is missing or invalid";
            return;
        }
        desired = jsonIterator->toObject();
    }

    emit client->desiredObjectChanged(desired);
}

void IotHubClient::reportedStateCallback(int statusCode, void *context)
{
    const auto client = static_cast<IotHubClient *>(context);
    emit client->deviceTwinUpdated(statusCode);
}

bool IotHubClient::sendMessage(int id, const QByteArray &data)
{

    const auto message = IoTHubMessage_CreateFromByteArray(
                reinterpret_cast<const unsigned char *>(data.constData()), data.size());

    const auto context = new MessageContext(this, id, message);

    const auto result = IoTHubDeviceClient_LL_SendEventAsync(mDeviceHandle, message, sendConfirmCallback, context);

    if (result != IOTHUB_CLIENT_OK) {
        IoTHubMessage_Destroy(message);
        delete context;
    }

    return result == IOTHUB_CLIENT_OK;
}

bool IotHubClient::updateDeviceTwin(const QJsonObject &reported)
{
    const auto reportedDocument = QJsonDocument(reported).toJson();
    const auto result = IoTHubDeviceClient_LL_SendReportedState(
                mDeviceHandle,
                reinterpret_cast<const unsigned char *>(reportedDocument.constData()),
                reportedDocument.size(), reportedStateCallback, this);
    return result == IOTHUB_CLIENT_OK;
}

double nextRandomValue(double current, double min, double max, double maxIncrement) {
    if (current < min || current > max)
        current = (min + max) / 2;

    const double factor = QRandomGenerator::global()->generateDouble() < 0.5 ? 1 : -1;
    const double increment = maxIncrement * QRandomGenerator::global()->generateDouble() * factor;

    if (current + increment > max || current + increment < min)
        return current - increment;

    return current + increment;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    IotHubClient client(&a);

    const auto idScope = QLatin1String("0ne000912D0");
    auto result = client.init(idScope);

    qDebug() << "Init:" << (result ? "successful" : "failed");

    if (!result)
        return 1;

    QObject::connect(&client, &IotHubClient::connectedChanged, [](bool connected) {
        qDebug() << "Client is now" << (connected ? "connected" : "disconnected");
    });

    QObject::connect(&client, &IotHubClient::deviceTwinUpdated, [](int statusCode) {
        qDebug() << "Device twin update returned" << statusCode;
    });

    QObject::connect(&client, &IotHubClient::messageStatusChanged, [](int id, bool success) {
        qDebug() << "Message" << id << (success ? "successfully sent" : "failed to send");
    });

    QTimer sendMessageTimer;
    int messageId = 0;

    // Initial values for the simulated data
    double humidity = 80;
    double pressure = 1000;
    double temperature = 23;
    double co2Level = 410;

    QObject::connect(&sendMessageTimer, &QTimer::timeout, &client, [&]() {
        qDebug() << "Message send timer triggered" << QDateTime::currentDateTime().toString(Qt::ISODate);
        auto environmentData = new iotexample::EnvironmentData();
        environmentData->set_humidity(nextRandomValue(humidity, 50, 100, 1));
        environmentData->set_pressure(nextRandomValue(pressure, 900, 1100, 1));
        environmentData->set_temperature(nextRandomValue(temperature, 10, 75, 1));
        environmentData->set_co2_level(nextRandomValue(co2Level,300, 500, 1));

        iotexample::DeviceMessages message;
        auto currentMessage = message.add_telemetry_messages();
        currentMessage->set_timestamp(QDateTime::currentMSecsSinceEpoch());
        currentMessage->set_allocated_environment_data(environmentData);

        // 1% chance of generating an error
        if (QRandomGenerator::global()->generateDouble() < 0.01) {
            qDebug() << "Generate error message";
            auto event = new iotexample::Event();
            event->set_message("Simulated event");
            event->set_error_level(static_cast<iotexample::ErrorLevel>(QRandomGenerator::global()->bounded(1, 4)));
            event->set_event_number(QRandomGenerator::global()->bounded(1, 10));

            currentMessage = message.add_telemetry_messages();
            currentMessage->set_timestamp(QDateTime::currentMSecsSinceEpoch());
            currentMessage->set_allocated_event(event);
        }

        qDebug() << "Serialized size:" << message.ByteSizeLong() << "bytes";

        QByteArray serializedMessage(message.ByteSizeLong(), Qt::Initialization::Uninitialized);
        auto success = message.SerializeToArray(serializedMessage.data(), message.ByteSizeLong());

        if (success) {
            client.sendMessage(++messageId, serializedMessage);
        } else {
            qWarning() << "Message serialization failed";
        }
    });

    QObject::connect(&client, &IotHubClient::desiredObjectChanged, [&](const QJsonObject &desired) {
        const auto sendIntervalKey = QLatin1String("sendInterval");

        auto jsonIterator = desired.constFind(sendIntervalKey);

        if (jsonIterator != desired.constEnd() && jsonIterator->isDouble() && jsonIterator->toInt(-1) > 0) {
            const auto sendInterval = jsonIterator->toInt();

            qDebug() << "Got send interval" << sendInterval << "from device twin";

            QJsonObject reported;
            reported[sendIntervalKey] = sendInterval;

            client.updateDeviceTwin(reported);

            // Initialize the send timer with the new interval
            sendMessageTimer.start(sendInterval * 1000);
        }
    });

    return a.exec();
}

#include "main.moc"
