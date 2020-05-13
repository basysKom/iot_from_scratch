QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

# Change /opt/azure/ to the location where azure-iot-sdk-c is installed
QMAKE_CXXFLAGS += -I/opt/azure/include/
QMAKE_CXXFLAGS += -I/opt/azure/include/azureiot
LIBS += -L/opt/azure/lib

HEADERS += \
        generated/environment_iot_messages.pb.h

SOURCES += \
        main.cpp \
        generated/environment_iot_messages.pb.cc

# Linker flags for Protobuf
LIBS += -lprotobuf

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

PROTO_COMPILER = protoc
system($$PROTO_COMPILER --version) {
  PROTOFILES = $$files(*.proto, true)
  for (PROTO_FILE, PROTOFILES) {
    command = $$PROTO_COMPILER --cpp_out=generated $$PROTO_FILE
    system($$command)
  }
} else {
    error("protoc is required to build this application")
}
