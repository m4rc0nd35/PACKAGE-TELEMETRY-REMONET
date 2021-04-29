CC = gcc
OBJS = paho/transport.o paho/MQTTConnectClient.o paho/MQTTSerializePublish.o paho/MQTTPacket.o paho/MQTTSubscribeClient.o paho/MQTTDeserializePublish.o paho/MQTTConnectServer.o paho/MQTTSubscribeServer.o paho/MQTTUnsubscribeServer.o paho/MQTTUnsubscribeClient.o

all: remonet pk

%.o: %.c
	$(CC) $(CFLAGS) -Ipaho -c -o $@ $<

remonet: $(OBJS) MQTTClient.o remonet.o
	$(CC) $(LDFLAGS) -o $@ $(OBJS) MQTTClient.o remonet.o -lmbedtls -lmbedx509 -lmbedcrypto

pk: pk.o
	$(CC) $(LDFLAGS) -o $@ pk.o -lmbedtls -lmbedx509 -lmbedcrypto

clean:
	rm *.o paho/*.o remonet pk
