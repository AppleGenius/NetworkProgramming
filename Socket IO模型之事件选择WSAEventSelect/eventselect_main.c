/*
WSAEventSelect��WSAAsyncSelectģ�����ƣ���Ҳ����Ӧ�ó�����һ�������׽����ϣ��������¼�Ϊ�����������¼�֪ͨ��
����WSAAsyncSelectģ�Ͳ��õ������¼���˵�����Ǿ���ԭ�ⲻ������ֲ���¼�ѡ��ģ���ϡ�
�����¼�ѡ��ģ�Ϳ�����Ӧ�ó����У�Ҳ�ܽ��պʹ���������Щ�¼���
��ģ������Ҫ�Ĳ�����������¼���Ͷ����һ���¼�������������Ͷ����һ���������̡� 


�¼�ѡ��ģ��Ҳ�Ƚϼ򵥣�ʵ������Ҳ����̫���ӣ����Ļ���˼���ǽ�ÿ���׽��ֶ���һ��WSAEVENT�����Ӧ������
�����ڹ�����ʱ��ָ����Ҫ��ע����Щ�����¼���һ����ĳ���׽����Ϸ��������ǹ�ע���¼���FD_READ��FD_CLOSE����
��֮�������WSAEVENT����Signaled��������������ȫ�����飬һ���׽������飬һ��WSAEVENT�������飬
���С����MAXIMUM_WAIT_OBJECTS��64�������������е�Ԫ��һһ��Ӧ�� 
ͬ���ģ�����ĳ���û�п����������⣬һ�ǲ����������ĵ���accept��
��Ϊ����֧�ֵĲ������������ޡ���������ǽ��׽��ְ� MAXIMUM_WAIT_OBJECTS���飬
ÿMAXIMUM_WAIT_OBJECTS���׽���һ�飬ÿһ�����һ���������̣߳����߲��� WSAAccept����accept��
���ص��Լ������Condition Function���ڶ���������û�ж�������Ϊ0�����������⴦��������������Ϊ0��ʱ��CPUռ����Ϊ100%��
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