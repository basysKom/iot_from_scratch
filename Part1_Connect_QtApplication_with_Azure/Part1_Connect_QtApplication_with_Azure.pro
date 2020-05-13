QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

# Change /opt/azure/ to the location where azure-iot-sdk-c is installed
QMAKE_CXXFLAGS += -I/opt/azure/include/
QMAKE_CXXFLAGS += -I/opt/azure/include/azureiot
LIBS += -L/opt/azure/lib

SOURCES += \
        main.cpp

# Flags for the static version of azure-iot-sdk-c
LIBS += \
    -liothub_client \
    -liothub_client_amqp_transport \
    -laziotsharedutil \
    -liothub_service_client \
    -lparson \
    -lserializer \
    -luamqp \
    -lumock_c \
    -lcurl \
    -lssl \
    -lcrypto \
    -luuid
