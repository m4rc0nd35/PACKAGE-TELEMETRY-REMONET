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
 * 	  M4rc0nd35 - Reconnection and send message to server
 *******************************************************************************/

#include <stdio.h>
#include <memory.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>

#include <MQTTClient.h>
#include <anlix-mqtt-transport.h>

#include <signal.h>
#include <sys/time.h>

volatile int toStop = 0;
Network network;
MQTTClient mqtt;

void usage()
{
	printf("MQTT stdout subscriber\n");
	printf("Usage: stdoutsub topicname <options>, where options are:\n");
	printf("  --host <hostname> (default is localhost)\n");
	printf("  --port <port> (default is 1883)\n");
	printf("  --qos <qos> (default is 2)\n");
	printf("  --delimiter <delim> (default is \\n)\n");
	printf("  --clientid <clientid> (default is hostname+timestamp)\n");
	printf("  --username none\n");
	printf("  --password none\n");
	printf("  --cafile none\n");
	printf("  --shell none (max 256 chars)\n");
	printf("  --debug Send debug messages to console (default to syslog)\n");
	exit(-1);
}

void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
}

struct opts_struct
{
	char *clientid;
	int nodelimiter;
	char *delimiter;
	enum QoS qos;
	char *username;
	char *password;
	char *host;
	char *cafile;
	char *shell;
	int port;
	int debug;
} opts = {(char *)"stdout-subscriber", 0, (char *)"\n", QOS2, NULL, NULL, (char *)"localhost", NULL, NULL, 1883, 0};

void getopts(int argc, char **argv)
{
	int count = 2;

	while (count < argc)
	{
		if (strcmp(argv[count], "--qos") == 0)
		{
			if (++count < argc)
			{
				if (strcmp(argv[count], "0") == 0)
					opts.qos = QOS0;
				else if (strcmp(argv[count], "1") == 0)
					opts.qos = QOS1;
				else if (strcmp(argv[count], "2") == 0)
					opts.qos = QOS2;
				else
					usage();
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--host") == 0)
		{
			if (++count < argc)
				opts.host = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--port") == 0)
		{
			if (++count < argc)
				opts.port = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--clientid") == 0)
		{
			if (++count < argc)
				opts.clientid = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--username") == 0)
		{
			if (++count < argc)
				opts.username = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--password") == 0)
		{
			if (++count < argc)
				opts.password = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--cafile") == 0)
		{
			if (++count < argc)
				opts.cafile = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--shell") == 0)
		{
			if (++count < argc)
			{
				opts.shell = argv[count];
				if (strlen(opts.shell) > 256)
					usage();
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--delimiter") == 0)
		{
			if (++count < argc)
				opts.delimiter = argv[count];
			else
				opts.nodelimiter = 1;
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
	int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
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
	int pid;

	char buffer[256];
	memset(buffer, 0, 256);
	snprintf(buffer, (int)(message->payloadlen) + 1 > 254 ? 254 : (int)(message->payloadlen) + 1, "%s", (char *)message->payload);
	printlog("Message Received (size %d bytes) (%s)", (int)message->payloadlen, buffer);
	// printf("buffer is :%s\n", exec("ifconfig"));
	if ((opts.shell != NULL) && (strlen(buffer) > 0))
	{
		char runsys[512];
		strcat(buffer, " &");
		sprintf(runsys, "%s ", opts.shell);
		strcat(runsys, buffer);
		printlog("Running (%s)", runsys);
		// int rc = system(runsys);
		char result[1024];
		char *command;
		strcpy(command, runsys);
		FILE *file = popen(command, "r");
		if (!file)
			printf("popen failed!");

		char return_cmm[128];
		while (!feof(file))
		{
			if (fgets(return_cmm, 128, file) != NULL)
			{
				printf("Len: %d Buffer: %s\n", strlen(return_cmm), return_cmm);
				strncat(result, return_cmm, strlen(return_cmm));
			}
		}
		char buf[1024];
		sprintf(buf, "%s", return_cmm);
		MQTTMessage send_msg;
		send_msg.qos = QOS0;
		send_msg.retained = '0';
		send_msg.dup = '0';
		send_msg.payload = (void *)buf;
		send_msg.payloadlen = strlen(buf) + 1;
		MQTTPublish(&mqtt, "/commands", &send_msg);
	}
	else
	{
		printf("1 %.*s\n", md->topicName->lenstring.len, md->topicName->lenstring.data);
		printf("2 %.*s\n", (int)message->payloadlen, (char *)message->payload);
		printf("3 %.*s%s\n", (int)message->payloadlen, (char *)message->payload, opts.delimiter);
	}

	// pclose(file);
}

int main(int argc, char **argv)
{
	while (1)
	{
		toStop = 0;
		int rc = 0;
		unsigned char buf[100];
		unsigned char readbuf[100];

		if (argc < 2)
			usage();

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
		data.clientID.cstring = opts.clientid;
		data.username.cstring = opts.username;
		data.password.cstring = opts.password;

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

		printlog("Subscribing to %s", topic);
		rc = MQTTSubscribe(&mqtt, topic, opts.qos, messageArrived);
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
	}

	return 0;
}