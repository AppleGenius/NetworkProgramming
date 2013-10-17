#include <WINSOCK2.H>
#include <stdio.h>
#include <string.h>

#define SERVER_ADDRESS "127.0.0.1"
#define PORT           5150
#define MSGSIZE        1024

#pragma comment(lib, "ws2_32.lib")

int main()
{
	WSADATA     wsaData;
	SOCKET      sClient;
	SOCKADDR_IN server;
	char        szMessage[MSGSIZE];
	int         ret;

	// Initialize Windows socket library
	WSAStartup(0x0202, &wsaData);

	// Create client socket
	sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Connect to server
	memset(&server, 0, sizeof(SOCKADDR_IN));
	server.sin_family = AF_INET;
	server.sin_addr.S_un.S_addr = inet_addr(SERVER_ADDRESS);
	server.sin_port = htons(PORT);

	connect(sClient, (struct sockaddr *)&server, sizeof(SOCKADDR_IN));

	while (TRUE)
	{
		printf("Send:");
		gets(szMessage);

		if(!strcmp(szMessage,"bye"))
			break;

		if(!strlen(szMessage))
			continue;
		// Send message
		send(sClient, szMessage, strlen(szMessage), 0);

		// Receive message
		ret = recv(sClient, szMessage, MSGSIZE, 0);
		szMessage[ret] = '\0';

		printf("Received [%d bytes]: '%s'\n", ret, szMessage);
	}

	// Clean up
	closesocket(sClient);
	WSACleanup();
	return 0;
}



