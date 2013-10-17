/*
����ɶ˿ڡ�ģ��������Ϊֹ��Ϊ���ӵ�һ��I/Oģ�͡�
Ȼ��������һ��Ӧ�ó���ͬʱ��Ҫ����Ϊ���ڶ���׽��֣���ô��������ģ�ͣ��������Դﵽ��ѵ�ϵͳ���ܣ�
�����ҵ��ǣ���ģ��ֻ������Windows NT��Windows 2000����ϵͳ��������Ƶĸ����ԣ�
ֻ�������Ӧ�ó�����Ҫͬʱ��������������ǧ���׽��ֵ�ʱ�򣬶���ϣ������ϵͳ�ڰ�װ��CPU���������࣬
Ӧ�ó��������Ҳ����������������Ӧ���ǲ��á���ɶ˿ڡ�ģ�͡�Ҫ��ס��һ������׼���ǣ�
����ҪΪWindows NT��Windows 2000���������ܵķ�����Ӧ�ã�ͬʱϣ��Ϊ�����׽���I/O�����ṩ����Web�����������ⷽ��ĵ������ӣ���
��ôI/O��ɶ˿�ģ�ͱ������ѡ�� 

�������˵���Ҫ���̣� 
1.������ɶ˿ڶ��� 
2.�����������̣߳����﹤�����̵߳������ǰ���CPU�ĸ����������ģ��������Դﵽ������ܣ� 
3.���������׽��֣��󶨣�������Ȼ��������ѭ�� 
4.��ѭ���У����������¼������飺 
(1).����һ���ͻ������� 
(2).���ÿͻ����׽�������ɶ˿ڰ󶨵�һ��(���ǵ���CreateIoCompletionPort������ε����ò�ͬ)��
	ע�⣬��������������ʱ���ݸ�CreateIoCompletionPort�ĵ���������Ӧ����һ����ɼ���
	һ�������������Ǵ���һ����������ݽṹ�ĵ�ַ���õ�������ݰ����˺͸ÿͻ��������йص���Ϣ��
	��������ֻ�����׽��־��������ֱ�ӽ��׽��־����Ϊ��ɼ����ݣ� 
(3).����һ��WSARecv�첽���ã�������õ��ˡ�β�����ݡ���ʹ�����������õĻ�����������WSAOVERLAPPED����֮��
	���⣬���в������͵���Ҫ��Ϣ�� 

�ڹ������̵߳�ѭ���У����� 
1.����GetQueuedCompletionStatusȡ�ñ���I/O�������Ϣ�������׽��־�������͵��ֽ�������I/O���ݽṹ�ĵ�ַ�ȵȣ� 
2.ͨ����I/O���ݽṹ�ҵ��������ݻ�������Ȼ������ԭ�ⲻ���ķ��͵��ͻ��� 
3.�ٴδ���һ��WSARecv�첽����
*/
#include "stdafx.h"  
#include <WINSOCK2.H>  
#include <stdio.h>  
#pragma comment(lib, "ws2_32.lib")  
#define PORT  5150  
#define MSGSIZE  1024  
typedef enum  
{  
	RECV_POSTED  
} OPERATION_TYPE;  
typedef struct   
{  
	WSAOVERLAPPED  overlap;  
	WSABUF         Buffer;  
	char           szMessage[MSGSIZE];  
	DWORD          NumberOfBytesRecvd;  
	DWORD          Flags;  
	OPERATION_TYPE OperationType;  
} PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;  
DWORD WINAPI WorkerThread(LPVOID CompletionPortID);  

int main(int argc, char* argv[])  
{  
	WSADATA wsaData;  
	SOCKET sListen, sClient;  
	SOCKADDR_IN local, client;  
	DWORD i, dwThreadId;  
	int iAddrSize = sizeof(SOCKADDR_IN);  
	HANDLE CompletionPort = INVALID_HANDLE_VALUE;  
	SYSTEM_INFO sysinfo;  
	LPPER_IO_OPERATION_DATA lpPerIOData = NULL;  
	// Initialize windows socket library  
	WSAStartup(0x0202, &wsaData);  
	// Create completion port  
	CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);  
	// Create worker thread  
	GetSystemInfo(&sysinfo);  
	for (i = 0; i < sysinfo.dwNumberOfProcessors; i++)  
	{  
		CreateThread(NULL, 0, WorkerThread, CompletionPort, 0, &dwThreadId);  
	}  
	// Create listening socket  
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  
	// Bind  
	local.sin_family = AF_INET;  
	local.sin_addr.S_un.S_addr = htonl(INADDR_ANY);  
	local.sin_port = htons(PORT);  
	bind(sListen, (sockaddr*)&local, sizeof(SOCKADDR_IN));  
	// Listen  
	listen(sListen, 3);  
	while (TRUE)  
	{  
		// Accept a connection  
		sClient = accept(sListen, (sockaddr*)&client, &iAddrSize);  
		printf("Accepted client:%s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));        
		// Associate the newly arrived client socket with completion port  
		CreateIoCompletionPort((HANDLE)sClient, CompletionPort, (DWORD)sClient, 0);  
		// Launch an asynchronous operation for new arrived connection  
		lpPerIOData = (LPPER_IO_OPERATION_DATA)HeapAlloc(  
			GetProcessHeap(),  
			HEAP_ZERO_MEMORY,  
			sizeof(PER_IO_OPERATION_DATA));  
		lpPerIOData->Buffer.len = MSGSIZE;  
		lpPerIOData->Buffer.buf = lpPerIOData->szMessage;  
		lpPerIOData->OperationType = RECV_POSTED;  
		WSARecv(sClient,  
			&lpPerIOData->Buffer,  
			1,  
			&lpPerIOData->NumberOfBytesRecvd,  
			&lpPerIOData->Flags,  
			&lpPerIOData->overlap,  
			NULL);  
	}  
	PostQueuedCompletionStatus(CompletionPort, 0xFFFFFFFF, 0, NULL);  
	CloseHandle(CompletionPort);  
	closesocket(sListen);  
	WSACleanup();  
	return 0;  
}  
DWORD WINAPI WorkerThread(LPVOID CompletionPortID)  
{  
	HANDLE CompletionPort = (HANDLE)CompletionPortID;  
	DWORD dwBytesTransferred;  
	SOCKET sClient;  
	LPPER_IO_OPERATION_DATA lpPerIOData = NULL;  
	while (TRUE)  
	{  
		GetQueuedCompletionStatus(  
			CompletionPort,  
			&dwBytesTransferred,  
			(DWORD*)&sClient,  
			(LPOVERLAPPED*)&lpPerIOData,  
			INFINITE);  
		if (dwBytesTransferred == 0xFFFFFFFF)  
		{  
			return 0;  
		}  
		if (lpPerIOData->OperationType == RECV_POSTED)  
		{  
			if (dwBytesTransferred == 0)  
			{  
				// Connection was closed by client  
				closesocket(sClient);  
				HeapFree(GetProcessHeap(), 0, lpPerIOData);  
			}  
			else  
			{  
				lpPerIOData->szMessage[dwBytesTransferred] = '\0';  
				send(sClient, lpPerIOData->szMessage, dwBytesTransferred, 0);  

				// Launch another asynchronous operation for sClient  
				memset(lpPerIOData, 0, sizeof(PER_IO_OPERATION_DATA));  
				lpPerIOData->Buffer.len = MSGSIZE;  
				lpPerIOData->Buffer.buf = lpPerIOData->szMessage;  
				lpPerIOData->OperationType = RECV_POSTED;  
				WSARecv(sClient,  
					&lpPerIOData->Buffer,  
					1,  
					&lpPerIOData->NumberOfBytesRecvd,  
					&lpPerIOData->Flags,  
					&lpPerIOData->overlap,  
					NULL);  
			}  
		}  
	}  
	return 0;  
}  