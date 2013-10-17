/*
ʹ���첽ѡ��ģ�ͣ�Ӧ�ó������һ���׽����ϣ�������Windows��ϢΪ�����������¼�֪ͨ��
������������ڽ���һ���׽��ֺ󣬵���WSAAsyncSelect������

�������˵���Ҫ���̣� 
1.��WM_CREATE��Ϣ�������У���ʼ��Windows Socket library�����������׽��֣��󶨣�������
  ���ҵ���WSAAsyncSelect������ʾ���ǹ����ڼ����׽����Ϸ�����FD_ACCEPT�¼��� 
2.�Զ���һ����ϢWM_SOCKET��һ�������������ĵ��׽��֣������׽��ֺͿͻ����׽��֣��Ϸ�����ĳ���¼���
  ϵͳ�ͻ����WndProc����message����������ΪWM_SOCKET�� 
3.��WM_SOCKET����Ϣ�������У��ֱ��FD_ACCEPT��FD_READ��FD_CLOSE�¼����д��� 
4.�ڴ���������Ϣ(WM_DESTROY)�Ĵ������У����ǹرռ����׽��֣����Windows Socket library 

WSAAsyncSelect�����������¼����Ϳ���������һ�֣� 
FD_READ Ӧ�ó�����Ҫ�����й��Ƿ�ɶ���֪ͨ���Ա�������� 
FD_WRITE Ӧ�ó�����Ҫ�����й��Ƿ��д��֪ͨ���Ա�д������ 
FD_OOB Ӧ�ó���������Ƿ��д��⣨OOB�����ݵִ��֪ͨ 
FD_ACCEPT Ӧ�ó������������������йص�֪ͨ 
FD_CONNECT Ӧ�ó����������һ�����ӻ��߶��join������ɵ�֪ͨ 
FD_CLOSE Ӧ�ó�����������׽��ֹر��йص�֪ͨ 
FD_QOS Ӧ�ó���������׽��֡�������������QoS���������ĵ�֪ͨ 
FD_GROUP_QOS  Ӧ�ó���������׽����顰�����������������ĵ�֪ͨ������ûʲô�ô���Ϊδ���׽������ʹ�ñ����� 
FD_ROUTING_INTERFACE_CHANGE Ӧ�ó����������ָ���ķ����ϣ���·�ɽӿڷ����仯��֪ͨ 
FD_ADDRESS_LIST_CHANGE  Ӧ�ó������������׽��ֵ�Э����壬���ص�ַ�б����仯��֪ͨ
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