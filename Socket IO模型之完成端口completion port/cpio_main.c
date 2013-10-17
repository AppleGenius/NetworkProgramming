/*
“完成端口”模型是迄今为止最为复杂的一种I/O模型。
然而，假若一个应用程序同时需要管理为数众多的套接字，那么采用这种模型，往往可以达到最佳的系统性能！
但不幸的是，该模型只适用于Windows NT和Windows 2000操作系统。因其设计的复杂性，
只有在你的应用程序需要同时管理数百乃至上千个套接字的时候，而且希望随着系统内安装的CPU数量的增多，
应用程序的性能也可以线性提升，才应考虑采用“完成端口”模型。要记住的一个基本准则是，
假如要为Windows NT或Windows 2000开发高性能的服务器应用，同时希望为大量套接字I/O请求提供服务（Web服务器便是这方面的典型例子），
那么I/O完成端口模型便是最佳选择！ 

服务器端得主要流程： 
1.创建完成端口对象 
2.创建工作者线程（这里工作者线程的数量是按照CPU的个数来决定的，这样可以达到最佳性能） 
3.创建监听套接字，绑定，监听，然后程序进入循环 
4.在循环中，我做了以下几件事情： 
(1).接受一个客户端连接 
(2).将该客户端套接字与完成端口绑定到一起(还是调用CreateIoCompletionPort，但这次的作用不同)，
	注意，按道理来讲，此时传递给CreateIoCompletionPort的第三个参数应该是一个完成键，
	一般来讲，程序都是传递一个单句柄数据结构的地址，该单句柄数据包含了和该客户端连接有关的信息，
	由于我们只关心套接字句柄，所以直接将套接字句柄作为完成键传递； 
(3).触发一个WSARecv异步调用，这次又用到了“尾随数据”，使接收数据所用的缓冲区紧跟在WSAOVERLAPPED对象之后，
	此外，还有操作类型等重要信息。 

在工作者线程的循环中，我们 
1.调用GetQueuedCompletionStatus取得本次I/O的相关信息（例如套接字句柄、传送的字节数、单I/O数据结构的地址等等） 
2.通过单I/O数据结构找到接收数据缓冲区，然后将数据原封不动的发送到客户端 
3.再次触发一个WSARecv异步操作
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