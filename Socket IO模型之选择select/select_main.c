/*
在windows平台构建网络应用，必须了解socket I/O模型。windows提供了选择(select)、异步选择(WSAAsyncSelect)、
事件选择(WSAEventSelect)、重叠I /O(overlapped I/O)和完成端口(completion port)。 

这是异步模型中最简单的一种，服务器端的几个主要流程如下： 

1.创建监听套接字，绑定，监听； 
2.创建工作者线程； 
3.创建一个套接字数组，用来存放当前所有活动的客户端套接字，每accept一个连接就更新一次数组； 
4.接受客户端的连接。
*/
#include "stdafx.h"  
#include <winsock.h>  
#include <stdio.h>  
#define PORT  5150  
#define MSGSIZE  1024  
#pragma comment(lib, "ws2_32.lib")  
int g_iTotalConn = 0;  
SOCKET g_CliSocketArr[FD_SETSIZE];  
DWORD WINAPI WorkerThread(LPVOID lpParam);  
int main(int argc, char* argv[])  
{  
	WSADATA wsaData;  
	SOCKET sListen, sClient;  
	SOCKADDR_IN local, client;  
	int iAddrSize = sizeof(SOCKADDR_IN);  
	DWORD dwThreadId;  
	// Initialize windows socket library  
	WSAStartup(0x0202, &wsaData);  
	// Create listening socket  
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  
	// Bind  
	local.sin_family = AF_INET;  
	local.sin_addr.S_un.S_addr = htonl(INADDR_ANY);  
	local.sin_port = htons(PORT);  
	bind(sListen, (sockaddr*)&local, sizeof(SOCKADDR_IN));  
	// Listen  
	listen(sListen, 3);  
	// Create worker thread  
	CreateThread(NULL, 0, WorkerThread, NULL, 0, &dwThreadId);  
	while (TRUE)   
	{  
		// Accept a connection  
		sClient = accept(sListen, (sockaddr*)&client, &iAddrSize);  
		printf("Accepted client:%s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));  
		// Add socket to g_CliSocketArr  
		g_CliSocketArr[g_iTotalConn++] = sClient;  
	}  
	return 0;  
}  
DWORD WINAPI WorkerThread(LPVOID lpParam)  
{  
	int i;  
	fd_set fdread;  
	int ret;  
	struct timeval tv = {1, 0};  
	char szMessage[MSGSIZE];  
	while (TRUE)   
	{  
		FD_ZERO(&fdread);  
		for (i = 0; i < g_iTotalConn; i++)   
		{  
			FD_SET(g_CliSocketArr[i], &fdread);  
		}  
		// We only care read event  
		ret = select(0, &fdread, NULL, NULL, &tv);  
		if (ret == 0)   
		{  
			// Time expired  
			continue;  
		}  
		for (i = 0; i < g_iTotalConn; i++)   
		{  
			if (FD_ISSET(g_CliSocketArr[i], &fdread))   
			{  
				// A read event happened on g_CliSocketArr  
				ret = recv(g_CliSocketArr[i], szMessage, MSGSIZE, 0);  
				if (ret == 0 || (ret == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET))   
				{  
					// Client socket closed  
					printf("Client socket %d closed.\n", g_CliSocketArr[i]);  
					closesocket(g_CliSocketArr[i]);  
					if (i < g_iTotalConn-1)   
					{  
						g_CliSocketArr[i--] = g_CliSocketArr[--g_iTotalConn];  
					}  
				}   
				else   
				{  
					// We reveived a message from client  
					szMessage[ret] = '\0';  
					send(g_CliSocketArr[i], szMessage, strlen(szMessage), 0);  
				}  
			}  
		}  
	}  
}  