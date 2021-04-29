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
	(char *)"mqtt-srv1.remonet.com.br",
	NULL,
	1883,
	0
};

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

void messageArrived(MessageData *md)
{
	MQTTMessage *message = md->message;

	char buffer[256];
	memset(buffer, 0, 256);
	snprintf(buffer, (int)(message->payloadlen) + 1 > 254 ? 254 : (int)(message->payloadlen) + 1, "%s", (char *)message->payload);

	char runsys[512];
	sprintf(runsys, "%s ", "sh /usr/share/mqtt.sh");
	strcat(runsys, buffer);
	printlog("Running (%s)", runsys);
	
	char result[1024];
	FILE *file = popen(runsys, "r");
	if (!file)
		printf("popen failed!");

	char return_cmm[128];
	while (!feof(file))
	{
		if (fgets(return_cmm, 128, file) != NULL)
		{
			strncat(result, return_cmm, strlen(return_cmm));
		}
	}
	pclose(file);

	char buf[strlen(return_cmm)];
	sprintf(buf, "%s", return_cmm);
	MQTTMessage send_msg;
	send_msg.qos = QOS0;
	send_msg.retained = '0';
	send_msg.dup = '0';
	send_msg.payload = (void *)buf;
	send_msg.payloadlen = strlen(buf) + 1;
	MQTTPublish(&mqtt, "/telemetry", &send_msg);

	printf("messageArrived %.*s\n", (int)strlen(return_cmm), (char *)return_cmm);
}

int main(int argc, char **argv)
{
	while (1)
	{
		toStop = 0;
		int rc = 0;
		unsigned char buf[100];
		unsigned char readbuf[100];

		char *topic = argv[1];

		getopts(argc, argv);

		if (!opts.debug)
			openlog("MQTT", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

		printlog("topic is %s", topic);

		signal(SIGINT, cfinish);
		signal(SIGTERM, cfinish);

		NetworkInit(&network);
		NetworkConnect(&network, opts.host, opts.port, opts.cafile);
		MQTTClientInit(&mqtt, &network, 1000, buf, 100, readbuf, 100);

		MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
		data.willFlag = 0;
		data.MQTTVersion = 3;
		data.clientID.cstring = (char*)"B8:27:EB:E8:64:48";
		data.username.cstring = (char*)"radios";
		data.password.cstring = (char*)"projecttrampolimdavitoria";

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

		rc = MQTTSubscribe(&mqtt, "/commands/all", QOS0, messageArrived);
		if (rc < 0)
		{
			// Error on connect, print error
			printlog("ERROR: %s", strerror(errno));
			exit(-1);
		}

		while (!toStop)
		{
			rc = MQTTYield(&mqtt, 30000);
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

		if (strcmp(opts.host, "mqtt-srv1.remonet.com.br") == 0)
			opts.host = "mqtt-srv2.remonet.com.br";
		else
			opts.host = "mqtt-srv1.remonet.com.br";
	}

	return 0;
}