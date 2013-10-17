/*
使用异步选择模型，应用程序可在一个套接字上，接收以Windows消息为基础的网络事件通知。
具体的做法是在建好一个套接字后，调用WSAAsyncSelect函数。

服务器端得主要流程： 
1.在WM_CREATE消息处理函数中，初始化Windows Socket library，创建监听套接字，绑定，监听，
  并且调用WSAAsyncSelect函数表示我们关心在监听套接字上发生的FD_ACCEPT事件； 
2.自定义一个消息WM_SOCKET，一旦在我们所关心的套接字（监听套接字和客户端套接字）上发生了某个事件，
  系统就会调用WndProc并且message参数被设置为WM_SOCKET； 
3.在WM_SOCKET的消息处理函数中，分别对FD_ACCEPT、FD_READ和FD_CLOSE事件进行处理； 
4.在窗口销毁消息(WM_DESTROY)的处理函数中，我们关闭监听套接字，清除Windows Socket library 

WSAAsyncSelect函数的网络事件类型可以有以下一种： 
FD_READ 应用程序想要接收有关是否可读的通知，以便读入数据 
FD_WRITE 应用程序想要接收有关是否可写的通知，以便写入数据 
FD_OOB 应用程序想接收是否有带外（OOB）数据抵达的通知 
FD_ACCEPT 应用程序想接收与进入连接有关的通知 
FD_CONNECT 应用程序想接收与一次连接或者多点join操作完成的通知 
FD_CLOSE 应用程序想接收与套接字关闭有关的通知 
FD_QOS 应用程序想接收套接字“服务质量”（QoS）发生更改的通知 
FD_GROUP_QOS  应用程序想接收套接字组“服务质量”发生更改的通知（现在没什么用处，为未来套接字组的使用保留） 
FD_ROUTING_INTERFACE_CHANGE 应用程序想接收在指定的方向上，与路由接口发生变化的通知 
FD_ADDRESS_LIST_CHANGE  应用程序想接收针对套接字的协议家族，本地地址列表发生变化的通知
*/


#include "stdafx.h"  
#include <winsock.h>  
#include <tchar.h>  
#define PORT  5150  
#define MSGSIZE  1024  
#define WM_SOCKET  WM_USER+0  
#pragma comment(lib, "ws2_32.lib")  
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);  
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)  
{  
	static TCHAR szAppName[] = _T("AsyncSelect Model");  
	HWND hwnd;  
	MSG msg;  
	WNDCLASS wndclass;  
	wndclass.style         = CS_HREDRAW | CS_VREDRAW;  
	wndclass.lpfnWndProc   = WndProc;  
	wndclass.cbClsExtra    = 0;  
	wndclass.cbWndExtra    = 0;  
	wndclass.hInstance     = hInstance;  
	wndclass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);  
	wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);  
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);  
	wndclass.lpszMenuName  = NULL;  
	wndclass.lpszClassName = szAppName;  
	if (!RegisterClass(&wndclass))  
	{  
		MessageBox(NULL, _T("This program requires Windows NT!"), szAppName, MB_ICONERROR);  
		return 0;  
	}  
	hwnd = CreateWindow(szAppName, // window class name  
		_T("AsyncSelect Model"),   // window caption  
		WS_OVERLAPPEDWINDOW,       // window stype  
		CW_USEDEFAULT,             // initial x postion  
		CW_USEDEFAULT,             // initial y postion  
		CW_USEDEFAULT,             // initial x size  
		CW_USEDEFAULT,             // initial y size  
		NULL,                      // parent window handle  
		NULL,                      // window menu handle  
		hInstance,                 // program instance handle  
		NULL);                     // creation parameters  
	ShowWindow(hwnd, nCmdShow);  
	UpdateWindow(hwnd);  
	while (GetMessage(&msg, NULL, 0, 0))  
	{  
		TranslateMessage(&msg);  
		DispatchMessage(&msg);  
	}  
	return msg.wParam;  
}  
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)  
{  
	WSADATA wsaData;  
	static SOCKET sListen;  
	SOCKET sClient;  
	SOCKADDR_IN local, client;  
	int ret, iAddrSize = sizeof(client);  
	char szMessage[MSGSIZE];  
	switch (message)  
	{  
	case WM_CREATE:  
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
		// Associate listening socket with FD_ACCEPT event  
		WSAAsyncSelect(sListen, hwnd, WM_SOCKET, FD_ACCEPT);  
		return 0;  
	case WM_DESTROY:  
		closesocket(sListen);  
		WSACleanup();  
		PostQuitMessage(0);  
		return 0;  
	case WM_SOCKET:  
		if (WSAGETSELECTERROR(lParam))  
		{  
			closesocket(wParam);  
			break;  
		}  
		switch (WSAGETSELECTEVENT(lParam))  
		{  
		case FD_ACCEPT:  
			// Accept a connection from client  
			sClient = accept(wParam, (sockaddr*)&client, &iAddrSize);  
			// Associate client socket with FD_READ and FD_CLOSE event  
			WSAAsyncSelect(sClient, hwnd, WM_SOCKET, FD_READ | FD_CLOSE);  
			break;  
		case FD_READ:  
			ret = recv(wParam, szMessage, MSGSIZE, 0);  
			if (ret == 0 || (ret == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET))  
			{  
				closesocket(wParam);  
			}  
			else  
			{  
				szMessage[ret] = '\0';  
				send(wParam, szMessage, strlen(szMessage), 0);  
			}  
			break;  
		case FD_CLOSE:  
			closesocket(wParam);  
			break;  
		}  
		return 0;  
	}  
	return DefWindowProc(hwnd, message, wParam, lParam);  
}  