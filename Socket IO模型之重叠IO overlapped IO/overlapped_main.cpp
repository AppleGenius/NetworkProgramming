/*

用事件通知方式实现的重叠I/O 模型 

Winsock2的发布使得Socket I/O有了和文件I/O统一的接口。我们可以通过使用Win32文件操纵函数ReadFile和WriteFile来进行Socket I/O。
伴随而来的，用于普通文件I/O的重叠I/O模型和完成端口模型对Socket I/O也适用了。
这些模型的优点是可以达到更佳的系统性能，但是实现较为复杂，里面涉及较多的C语言技巧。
例如我们在完成端口模型中会经常用到所谓的“尾随数据”。

这个模型与上述其他模型不同的是它使用Winsock2提供的异步I/O函数WSARecv。
在调用WSARecv时，指定一个 WSAOVERLAPPED 结构，这个调用不是阻塞的，也就是说，它会立刻返回。
一旦有数据到达的时候，被指定的WSAOVERLAPPED结构中的hEvent被 Signaled。由于下面这个语句 
g_CliEventArr[g_iTotalConn] = g_pPerIODataArr[g_iTotalConn]->overlap.hEvent； 
使得与该套接字相关联的WSAEVENT对象也被Signaled，所以WSAWaitForMultipleEvents的调用操作成功返回。
我们现在应该做的就是用与调用WSARecv相同的WSAOVERLAPPED结构为参数调用WSAGetOverlappedResult，
从而得到本次I/O 传送的字节数等相关信息。在取得接收的数据后，把数据原封不动的发送到客户端，然后重新激活一个WSARecv异步操作。 
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