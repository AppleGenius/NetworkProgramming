/*

用完成例程方式实现的重叠I/O 模型 

用完成例程来实现重叠I/O比用事件通知简单得多。在这个模型中，主线程只用不停的接受连接即可；
辅助线程判断有没有新的客户端连接被建立，如果有，就为那个客户端套接字激活一个异步的WSARecv操作，
然后调用SleepEx使线程处于一种可警告的等待状态，以使得I/O完成后 CompletionROUTINE可以被内核调用。
如果辅助线程不调用SleepEx，则内核在完成一次I/O操作后，
无法调用完成例程（因为完成例程的运行应该和当初激活WSARecv异步操作的代码在同一个线程之内）。 
完成例程内的实现代码比较简单，它取出接收到的数据，然后将数据原封不动的发送给客户端，最后重新激活另一个WSARecv异步操作。
注意，在这里用到了 “尾随数据”。我们在调用WSARecv的时候，
参数lpOverlapped实际上指向一个比它大得多的结构 PER_IO_OPERATION_DATA，这个结构除了WSAOVERLAPPED以外，
还被我们附加了缓冲区的结构信息，另外还包括客户端套接字等重要的信息。
这样，在完成例程中通过参数lpOverlapped拿到的不仅仅是WSAOVERLAPPED结构，还有后边尾随的包含客户端套接字和接收数据缓冲区等重要信息。
这样的C语言技巧在我后面介绍完成端口的时候还会使用到。

在事件驱动下：
ret = WSAWaitForMultipleEvents(g_iTotalConn, g_CliEventArr, FALSE, 1000, FALSE);    
if (ret == WSA_WAIT_FAILED || ret == WSA_WAIT_TIMEOUT)    
{    
//如果当前没有客户端的话，要Sleep一下，不然的话CPU会占50%以上  
if(g_iTotalConn == 0)   
Sleep(1000);  
continue;    
}    

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
	SOCKET        sClient;  
} PER_IO_OPERATION_DATA, *LPPER_IO_OPERATION_DATA;  
int                     g_iTotalConn = 0;  
SOCKET                  g_CliSocketArr[MAXIMUM_WAIT_OBJECTS];  
WSAEVENT                g_CliEventArr[MAXIMUM_WAIT_OBJECTS];  
LPPER_IO_OPERATION_DATA g_pPerIoDataArr[MAXIMUM_WAIT_OBJECTS];  
DWORD WINAPI WorkerThread(LPVOID lpParam);  
void CALLBACK CompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);  
SOCKET g_sNewClientConnection;  
BOOL g_bNewConnectionArrived = FALSE;  

int main(int argc, char* argv[])  
{  
	WSADATA wsaData;  
	SOCKET sListen;  
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
		g_sNewClientConnection = accept(sListen, (sockaddr*)&client, &iAddrSize);  
		g_bNewConnectionArrived = TRUE;  
		printf("Accepted client:%s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));        
	}  
	return 0;  
}  
DWORD WINAPI WorkerThread(LPVOID lpParam)  
{  
	LPPER_IO_OPERATION_DATA lpPerIOData = NULL;  
	while (TRUE)  
	{  
		if (g_bNewConnectionArrived)  
		{  
			// Launch an asynchronous operation for new arrived connection  
			lpPerIOData = (LPPER_IO_OPERATION_DATA)HeapAlloc(  
				GetProcessHeap(),  
				HEAP_ZERO_MEMORY,  
				sizeof(PER_IO_OPERATION_DATA));  
			lpPerIOData->Buffer.len = MSGSIZE;  
			lpPerIOData->Buffer.buf = lpPerIOData->szMessage;  
			lpPerIOData->sClient = g_sNewClientConnection;  
			WSARecv(lpPerIOData->sClient,  
				&lpPerIOData->Buffer,  
				1,  
				&lpPerIOData->NumberOfBytesRecvd,  
				&lpPerIOData->Flags,  
				&lpPerIOData->overlap,  
				CompletionRoutine);  
			g_bNewConnectionArrived = FALSE;  
		}  
		SleepEx(1000, TRUE);  
	}  
	return 0;  
}  
void CALLBACK CompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)  
{  
	LPPER_IO_OPERATION_DATA lpPerIOData = (LPPER_IO_OPERATION_DATA)lpOverlapped;  
	if (dwError != 0 || cbTransferred == 0)  
	{  
		// Connection was closed by client  
		closesocket(lpPerIOData->sClient);  
		HeapFree(GetProcessHeap(), 0, lpPerIOData);  
	}  
	else  
	{  
		lpPerIOData->szMessage[cbTransferred] = '\0';  
		send(lpPerIOData->sClient, lpPerIOData->szMessage, cbTransferred, 0);  
		// Launch another asynchronous operation  
		memset(&lpPerIOData->overlap, 0, sizeof(WSAOVERLAPPED));  
		lpPerIOData->Buffer.len = MSGSIZE;  
		lpPerIOData->Buffer.buf = lpPerIOData->szMessage;  
		WSARecv(lpPerIOData->sClient,  
			&lpPerIOData->Buffer,  
			1,  
			&lpPerIOData->NumberOfBytesRecvd,  
			&lpPerIOData->Flags,  
			&lpPerIOData->overlap,  
			CompletionRoutine);  
	}  
}  