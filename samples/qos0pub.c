/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "MQTTSNPacket.h"

#include <sys/types.h>

#if !defined(SOCKET_ERROR)
	/** error in socket operation */
	#define SOCKET_ERROR -1
#endif

#if defined(WIN32)
/* default on Windows is 64 - increase to make Linux and Windows the same */
#define FD_SETSIZE 1024
#include <winsock2.h>
#include <ws2tcpip.h>
#define MAXHOSTNAMELEN 256
#define EAGAIN WSAEWOULDBLOCK
#define EINTR WSAEINTR
#define EINVAL WSAEINVAL
#define EINPROGRESS WSAEINPROGRESS
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ENOTCONN WSAENOTCONN
#define ECONNRESET WSAECONNRESET
#define ioctl ioctlsocket
#define socklen_t int
#else
#define INVALID_SOCKET SOCKET_ERROR
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#endif

#if defined(WIN32)
#include <Iphlpapi.h>
#else
#include <sys/ioctl.h>
#include <net/if.h>
#endif


int Socket_error(char* aString, int sock)
{
#if defined(WIN32)
	int errno;
#endif

#if defined(WIN32)
	errno = WSAGetLastError();
#endif
	if (errno != EINTR && errno != EAGAIN && errno != EINPROGRESS && errno != EWOULDBLOCK)
	{
		if (strcmp(aString, "shutdown") != 0 || (errno != ENOTCONN && errno != ECONNRESET))
		{
			int orig_errno = errno;
			char* errmsg = strerror(errno);

			printf("Socket error %d (%s) in %s for socket %d\n", orig_errno, errmsg, aString, sock);
		}
	}
	return errno;
}


int sendPacketBuffer(int asocket, char* host, int port, unsigned char* buf, int buflen)
{
	struct sockaddr_in cliaddr;
	int rc = 0;

	memset(&cliaddr, 0, sizeof(cliaddr));
	cliaddr.sin_family = AF_INET;
	cliaddr.sin_addr.s_addr = inet_addr(host);
	cliaddr.sin_port = htons(port);

	if ((rc = sendto(asocket, buf, buflen, 0, (const struct sockaddr*)&cliaddr, sizeof(cliaddr))) == SOCKET_ERROR)
		Socket_error("sendto", asocket);
	else
		rc = 0;
	return rc;
}


int main(int argc, char** argv)
{
	int rc = 0;
	unsigned char buf[200];
	int buflen = sizeof(buf);
	int mysock = 0;
	MQTTSN_topicid topic;
	unsigned char* payload = (unsigned char*)"mypayload";
	int payloadlen = strlen((char*)payload);
	int len = 0;
	int dup = 0;
	int qos = 0;
	int retained = 0, packetid = 0;
	char *host = "127.0.0.1";
	char *topicname = "a long topic name";
	int port = 1883;
	MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;

	if (argc > 1)
		host = argv[1];

	if (argc > 2)
		port = atoi(argv[2]);

	printf("Sending to hostname %s port %d\n", host, port);

	mysock = socket(AF_INET, SOCK_DGRAM, 0);
	if (mysock == INVALID_SOCKET)
		rc = Socket_error("socket", mysock);

	options.clientID.cstring = "myclientid";
	len = MQTTSNSerialize_connect(buf, buflen, &options);
	rc = sendPacketBuffer(mysock, host, port, buf, len);

	topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
	topic.data.qos3.longname = topicname;
	topic.data.qos3.longlen = strlen(topicname);
	len = MQTTSNSerialize_publish(buf, buflen - len, dup, qos, retained, packetid,
			topic, payload, payloadlen);
	rc = sendPacketBuffer(mysock, host, port, buf, len);

	rc = shutdown(mysock, SHUT_WR);
	rc = close(mysock);

	return 0;
}