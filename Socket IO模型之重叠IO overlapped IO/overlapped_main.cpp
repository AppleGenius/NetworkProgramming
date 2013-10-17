/*

���¼�֪ͨ��ʽʵ�ֵ��ص�I/O ģ�� 

Winsock2�ķ���ʹ��Socket I/O���˺��ļ�I/Oͳһ�Ľӿڡ����ǿ���ͨ��ʹ��Win32�ļ����ݺ���ReadFile��WriteFile������Socket I/O��
��������ģ�������ͨ�ļ�I/O���ص�I/Oģ�ͺ���ɶ˿�ģ�Ͷ�Socket I/OҲ�����ˡ�
��Щģ�͵��ŵ��ǿ��Դﵽ���ѵ�ϵͳ���ܣ�����ʵ�ֽ�Ϊ���ӣ������漰�϶��C���Լ��ɡ�
������������ɶ˿�ģ���лᾭ���õ���ν�ġ�β�����ݡ���

���ģ������������ģ�Ͳ�ͬ������ʹ��Winsock2�ṩ���첽I/O����WSARecv��
�ڵ���WSARecvʱ��ָ��һ�� WSAOVERLAPPED �ṹ��������ò��������ģ�Ҳ����˵���������̷��ء�
һ�������ݵ����ʱ�򣬱�ָ����WSAOVERLAPPED�ṹ�е�hEvent�� Signaled���������������� 
g_CliEventArr[g_iTotalConn] = g_pPerIODataArr[g_iTotalConn]->overlap.hEvent�� 
ʹ������׽����������WSAEVENT����Ҳ��Signaled������WSAWaitForMultipleEvents�ĵ��ò����ɹ����ء�
��������Ӧ�����ľ����������WSARecv��ͬ��WSAOVERLAPPED�ṹΪ��������WSAGetOverlappedResult��
�Ӷ��õ�����I/O ���͵��ֽ����������Ϣ����ȡ�ý��յ����ݺ󣬰�����ԭ�ⲻ���ķ��͵��ͻ��ˣ�Ȼ�����¼���һ��WSARecv�첽������ 
*/

#include "stdafx.h"  
#include <WINSOCK2.H>  
#include <stdio.h>  
#pragma comment(lib, "ws2_32.lib")  
#define PORT  5150  
#define MSGSIZE  1024  
typedef struct   
{  
	WSAOVERLAPPED overlap;  
	WSABUF        Buffer;  
	char          szMessage[MSGSIZE];  
	DWORD         NumberOfBytesRecvd;  
	DWORD         Flags;  
} PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;  
int                     g_iTotalConn = 0;  
SOCKET                  g_CliSocketArr[MAXIMUM_WAIT_OBJECTS];  
WSAEVENT                g_CliEventArr[MAXIMUM_WAIT_OBJECTS];  
LPPER_IO_OPERATION_DATA g_pPerIoDataArr[MAXIMUM_WAIT_OBJECTS];  
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
		g_CliSocketArr[g_iTotalConn] = sClient;  
		// Associate a PER_IO_OPERATION_DATA structure  
		g_pPerIoDataArr[g_iTotalConn] = (LPPER_IO_OPERATION_DATA)HeapAlloc(  
			GetProcessHeap(),  
			HEAP_ZERO_MEMORY,  
			sizeof(PER_IO_OPERATION_DATA));  
		g_pPerIoDataArr[g_iTotalConn]->Buffer.len = MSGSIZE;  
		g_pPerIoDataArr[g_iTotalConn]->Buffer.buf = g_pPerIoDataArr[g_iTotalConn]->szMessage;  
		g_CliEventArr[g_iTotalConn] = g_pPerIoDataArr[g_iTotalConn]->overlap.hEvent = WSACreateEvent();  
		// Launch an asynchronous operation  
		WSARecv(g_CliSocketArr[g_iTotalConn],  
			&g_pPerIoDataArr[g_iTotalConn]->Buffer,  
			1,  
			&g_pPerIoDataArr[g_iTotalConn]->NumberOfBytesRecvd,  
			&g_pPerIoDataArr[g_iTotalConn]->Flags,  
			&g_pPerIoDataArr[g_iTotalConn]->overlap,  
			NULL);  
		g_iTotalConn++;  
	}  

	closesocket(sListen);  
	WSACleanup();  
	return 0;  
}  
DWORD WINAPI WorkerThread(LPVOID lpParam)  
{  
	int ret, index;  
	DWORD cbTransferred;  
	while (TRUE)  
	{  
		ret = WSAWaitForMultipleEvents(g_iTotalConn, g_CliEventArr, FALSE, 1000, FALSE);  
		if (ret == WSA_WAIT_FAILED || ret == WSA_WAIT_TIMEOUT)  
		{  
			continue;  
		}  
		index = ret - WSA_WAIT_EVENT_0;  
		WSAResetEvent(g_CliEventArr[index]);  
		WSAGetOverlappedResult(g_CliSocketArr[index],  
			&g_pPerIoDataArr[index]->overlap,  
			&cbTransferred,  
			TRUE,  
			&g_pPerIoDataArr[g_iTotalConn]->Flags);  
		if (cbTransferred == 0)  
		{  
			// The connection was closed by client  
			Cleanup(index);  
		}  
		else  
		{  
			// g_pPerIoDataArr[index]->szMessage contains the recvived data  
			g_pPerIoDataArr[index]->szMessage[cbTransferred] = '\0';  
			send(g_CliSocketArr[index], g_pPerIoDataArr[index]->szMessage, cbTransferred, 0);  
			// Launch another asynchronous operation  
			WSARecv(g_CliSocketArr[index],  
				&g_pPerIoDataArr[index]->Buffer,  
				1,  
				&g_pPerIoDataArr[index]->NumberOfBytesRecvd,  
				&g_pPerIoDataArr[index]->Flags,  
				&g_pPerIoDataArr[index]->overlap,  
				NULL);  
		}  
	}  

	return 0;  
}  
void Cleanup(int index)  
{  
	closesocket(g_CliSocketArr[index]);  
	WSACloseEvent(g_CliEventArr[index]);  
	HeapFree(GetProcessHeap(), 0, g_pPerIoDataArr[index]);  
	if (index < g_iTotalConn-1)  
	{  
		g_CliSocketArr[index] = g_CliSocketArr[g_iTotalConn-1];  
		g_CliEventArr[index] = g_CliEventArr[g_iTotalConn-1];  
		g_pPerIoDataArr[index] = g_pPerIoDataArr[g_iTotalConn-1];  
	}  

	g_pPerIoDataArr[--g_iTotalConn] = NULL;  
}  