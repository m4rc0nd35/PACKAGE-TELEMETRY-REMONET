/*******************************************************************************
 * Copyright (c) 2012, 2016 IBM Corp.
 * Copyright (c) 2018 Anlix
 * Copyrigth (c) 2021 Remonet
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *   http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial contribution
 *    Ian Craggs - change delimiter option from char to string
 *    Al Stockdill-Mander - Version using the embedded C client
 *    Ian Craggs - update MQTTClient function names
 *    Gaspare Bruno - Run script if a message is received, add syslog
 * 	  M4rc0nd35 - Reconnect to server
 * 	  M4rc0nd35 - Send message to server
 * 	  M4rc0nd35 - Fix crash connection
 *******************************************************************************/

#include <stdio.h>
#include <memory.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>

#include "MQTTClient.h"
#include "transport.h"

#include <signal.h>
#include <sys/time.h>

#define USER "radios"
#define TOPIC_RETURN "/telemetry"
#define TOPIC_COMMAND "/commands/"
#define PASSWORD "projecttrampolimdavitoria"
#define HOST_MASTER "mqtt-srv1.remonet.com.br"
#define HOST_SLAVE "mqtt-srv2.remonet.com.br"

volatile int toStop = 0;
Network network;
MQTTClient mqtt;

void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
}

struct opts_struct
{
	char *host;
	char *cafile;
	int port;
	int debug;
} opts = {
	(char *)HOST_MASTER,
	NULL,
	1883,
	0};

void getopts(int argc, char **argv)
{
	int count = 0;

	while (count < argc)
	{
		if (strcmp(argv[count], "--cafile") == 0)
		{
			opts.cafile = argv[count];
		}
		else if (strcmp(argv[count], "--debug") == 0)
		{
			opts.debug = 1;
		}
		count++;
	}
}

void printlog(const char *fmt, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	if (opts.debug)
	{
		printf("%s\n", buffer);
	}
	else
	{
		syslog(LOG_INFO, "%s", buffer);
	}
}

void shell(char *command)
{
	FILE *file = popen(command, "r");
	if (!file)
		printf("popen failed!");

	char return_cmm[4096];
	while (!feof(file))
		fgets(return_cmm, 4096, file);

	pclose(file);
	memset(command, 0, sizeof command);
	strcpy(command, return_cmm);
}

void messageArrived(MessageData *md)
{
	MQTTMessage *message = md->message;

	char buffer[256];
	memset(buffer, 0, 256);
	snprintf(buffer, (int)(message->payloadlen) + 1 > 254 ? 254 : (int)(message->payloadlen) + 1, "%s", (char *)message->payload);
	printlog("messageArrived %.*s\n", (int)strlen(buffer), (char *)buffer);

	char runsys[4096];
	sprintf(runsys, "%s ", "sh /usr/share/mqtt.sh");
	strcat(runsys, buffer);
	printlog("Running (%s)", runsys);

	shell(&runsys);

	MQTTMessage send_msg;
	send_msg.qos = QOS0;
	send_msg.retained = '0';
	send_msg.dup = '0';
	send_msg.payload = (void *)runsys;
	send_msg.payloadlen = strlen(runsys) + 1;
	MQTTPublish(&mqtt, TOPIC_RETURN, &send_msg);

	printlog("messageArrived %.*s\n", (int)send_msg.payloadlen, (char *)send_msg.payload);
}

int main(int argc, char **argv)
{
	while (1)
	{
		toStop = 0;
		int rc = 0;
		unsigned char buf[4096];
		unsigned char readbuf[4096];
		char clientID[18];
		memset(clientID, 0, sizeof clientID);

		getopts(argc, argv);

		if (!opts.debug)
			openlog("MQTT", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

		char mac[128] = "awk '{print toupper($1)}' /sys/class/net/eth0/address";
		// memset(mac, 0, sizeof mac);
		shell(&mac);

		signal(SIGINT, cfinish);
		signal(SIGTERM, cfinish);
		
		NetworkInit(&network);
		NetworkConnect(&network, opts.host, opts.port, opts.cafile);
		MQTTClientInit(&mqtt, &network, 1000, buf, 4096, readbuf, 4096);
		
		strncat(clientID, mac, strlen(mac)-1);
		MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
		data.willFlag = 0;
		data.MQTTVersion = 3;
		data.clientID.cstring = (char *)clientID;
		data.username.cstring = (char *)USER;
		data.password.cstring = (char *)PASSWORD;

		data.keepAliveInterval = 30;
		data.cleansession = 1;
		printlog("Connecting to %s %d", opts.host, opts.port);

		rc = MQTTConnect(&mqtt, &data);
		printlog("Connected with code %d", rc);
		if (rc < 0)
		{
			// Error on connect, print error
			printlog("ERROR: %s", strerror(errno));
			exit(-1);
		}
		char topic[] = "/commands/";
		strcat(topic, clientID);
		rc = MQTTSubscribe(&mqtt, topic, QOS0, messageArrived);
		printlog("TOPIC: %s",topic);
		rc = MQTTSubscribe(&mqtt, "/commands/all", QOS0, messageArrived);
		if (rc < 0)
		{
			// Error on connect, print error
			printlog("ERROR: %s", strerror(errno));
			exit(-1);
		}

		while (!toStop)
		{
			rc = MQTTYield(&mqtt, 1000);
			if (rc != SUCCESS)
			{
				toStop = 1;
				printlog("MQTT Yield exit (%d), disconnecting...", rc);
			}
		}

		printlog("Stopping");
		MQTTDisconnect(&mqtt);
		NetworkDisconnect(&network);

		if (!opts.debug)
			closelog();
		usleep(5 * 1000000);

		if (strcmp(opts.host, HOST_MASTER) == 0)
			opts.host = HOST_SLAVE;
		else
			opts.host = HOST_MASTER;
	}

	return 0;
}