/*
WSAEventSelect和WSAAsyncSelect模型类似，它也允许应用程序在一个或多个套接字上，接收以事件为基础的网络事件通知。
对于WSAAsyncSelect模型采用的网络事件来说，它们均可原封不动地移植到事件选择模型上。
在用事件选择模型开发的应用程序中，也能接收和处理所有那些事件。
该模型最主要的差别在于网络事件会投递至一个事件对象句柄，而非投递至一个窗口例程。 


事件选择模型也比较简单，实现起来也不是太复杂，它的基本思想是将每个套接字都和一个WSAEVENT对象对应起来，
并且在关联的时候指定需要关注的哪些网络事件。一旦在某个套接字上发生了我们关注的事件（FD_READ和FD_CLOSE），
与之相关联的WSAEVENT对象被Signaled。程序定义了两个全局数组，一个套接字数组，一个WSAEVENT对象数组，
其大小都是MAXIMUM_WAIT_OBJECTS（64），两个数组中的元素一一对应。 
同样的，这里的程序没有考虑两个问题，一是不能无条件的调用accept，
因为我们支持的并发连接数有限。解决方法是将套接字按 MAXIMUM_WAIT_OBJECTS分组，
每MAXIMUM_WAIT_OBJECTS个套接字一组，每一组分配一个工作者线程；或者采用 WSAAccept代替accept，
并回调自己定义的Condition Function。第二个问题是没有对连接数为0的情形做特殊处理，程序在连接数为0的时候CPU占用率为100%。
*/
#include "stdafx.h"  
#include <WINSOCK2.H>  
#include <stdio.h>  
#pragma comment(lib, "ws2_32.lib")  
#define PORT  5150  
#define MSGSIZE  1024  
int g_iTotalConn = 0;  
SOCKET g_CliSocketArr[MAXIMUM_WAIT_OBJECTS];  
WSAEVENT g_CliEventArr[MAXIMUM_WAIT_OBJECTS];  
DWORD WINAPI WorkerThread(LPVOID lpParam);  
void Cleanup(int index);  
int main(int argc, char* argv[])  
{  
	WSADATA wsaData;  
	SOCKET sListen, sClient;  
	SOCKADDR_IN local, client;  
	DWORD dwThreadId;  
	int iAddrSize = sizeof(SOCKADDR_IN);  
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
		// Associate socket with network event  
		g_CliSocketArr[g_iTotalConn] = sClient;  
		g_CliEventArr[g_iTotalConn] = WSACreateEvent();  
		WSAEventSelect(g_CliSocketArr[g_iTotalConn], g_CliEventArr[g_iTotalConn], FD_READ|FD_CLOSE);  
		g_iTotalConn++;  
	}  
	return 0;  
}  
DWORD WINAPI WorkerThread(LPVOID lpParam)  
{  
	int ret, index;  
	WSANETWORKEVENTS NetworkEvents;  
	char szMessage[MSGSIZE];  
	while (TRUE)  
	{  
		ret = WSAWaitForMultipleEvents(g_iTotalConn, g_CliEventArr, FALSE, 1000, FALSE);  
		if (ret == WSA_WAIT_FAILED || ret == WSA_WAIT_TIMEOUT)  
		{  
			continue;  
		}  
		index = ret - WSA_WAIT_EVENT_0;  
		WSAEnumNetworkEvents(g_CliSocketArr[index], g_CliEventArr[index], &NetworkEvents);  
		if (NetworkEvents.lNetworkEvents & FD_READ)  
		{  
			// Receive message from client  
			ret = recv(g_CliSocketArr[index], szMessage, MSGSIZE, 0);  
			if (ret == 0 || (ret == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET))  
			{  
				Cleanup(index);  
			}  
			else  
			{  
				szMessage[ret] = '\0';  
				send(g_CliSocketArr[index], szMessage, strlen(szMessage), 0);  
			}  
		}  
		if (NetworkEvents.lNetworkEvents & FD_CLOSE)  
		{  
			Cleanup(index);  
		}  
	}  
	return 0;  
}  
void Cleanup(int index)  
{  
	closesocket(g_CliSocketArr[index]);  
	WSACloseEvent(g_CliEventArr[index]);  
	if (index < g_iTotalConn-1)  
	{  
		g_CliSocketArr[index] = g_CliSocketArr[g_iTotalConn-1];  
		g_CliEventArr[index] = g_CliEventArr[g_iTotalConn-1];  
	}  
	g_iTotalConn--;  
} 