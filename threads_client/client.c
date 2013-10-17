// Client.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string.h>
#include <assert.h>
#include <iostream>
#include <winsock2.h>
#include <process.h>

#pragma comment(lib, "WS2_32.lib")
using namespace std;


#define CLIENT_SETUP_FAIL			1		//启动客户端失败
#define CLIENT_CREATETHREAD_FAIL	2		//创建线程失败
#define CLIENT_CONNECT_FAIL			3		//网络连接断开
#define TIMEFOR_THREAD_EXIT			1000	//子线程退出时间
#define TIMEFOR_THREAD_SLEEP		500		//线程睡眠时间

#define	SERVERIP			"127.0.0.1"		//服务器IP
#define	SERVERPORT			5556			//服务器TCP端口
#define	MAX_NUM_BUF			48				//缓冲区的最大长度
#define ADD					'+'				//+
#define SUB					'-'				//- 
#define MUT					'*'				//*
#define DIV					'/'				///
#define EQU					'='				//=

//数据包类型
#define EXPRESSION			'E'				//算数表达式
#define BYEBYE				'B'				//消息byebye
#define HEADERLEN			(sizeof(hdr))	//头长度

//数据包头结构该结构在win32xp下为4byte
typedef struct _head
{
	char			type;//类型		
	unsigned short	len;//数据包的长度(包括头的长度)
}hdr, *phdr;

//数据包中的数据结构
typedef struct _data 
{
	char	buf[MAX_NUM_BUF];
}DATABUF, *pDataBuf;

SOCKET	sClient;							//套接字
HANDLE	hThreadSend;						//发送数据线程
HANDLE	hThreadRecv;						//接收数据线程
DATABUF bufSend;							//发送数据缓冲区
DATABUF bufRecv;							//接收数据缓冲区
CRITICAL_SECTION csSend;					//临界区对象，锁定bufSend
CRITICAL_SECTION csRecv;					//临界区对象，锁定bufRecv
BOOL	bSendData;							//通知发送数据线程
HANDLE	hEventShowDataResult;				//显示计算结果的事件
BOOL	bConnecting;						//与服务器的连接状态
HANDLE	arrThread[2];						//子线程数组


BOOL	InitClient(void);					//初始化
BOOL	ConnectServer(void);				//连接服务器
BOOL	CreateSendAndRecvThread(void);		//创建发送和接收数据线程
void	InputAndOutput(void);				//用户输入数据
void	ExitClient(void);					//退出

void	InitMember(void);					//初始化全局变量
BOOL    InitSockt(void);					//创建SOCKET

DWORD __stdcall	RecvDataThread(void* pParam);		//接收数据线程
DWORD __stdcall	SendDataThread(void* pParam);		//发送数据线程

BOOL	PackByebye(const char* pExpr);		//将输入的"Byebye" "byebye"的字符串打包
BOOL	PackExpression(const char *pExpr);	//将输入的算数表达式打包

void	ShowConnectMsg(BOOL bSuc);			//显示连接服务器消息
void	ShowDataResultMsg(void);			//显示连计算结果
void	ShowTipMsg(BOOL bFirstInput);		//显示提示信息


int main(int argc, char* argv[])
{	
	//初始化
	if (!InitClient())
	{	
		ExitClient();
		return CLIENT_SETUP_FAIL;
	}	
	
	//连接服务器
	if (ConnectServer())
	{
		ShowConnectMsg(TRUE);	
	}else{
		ShowConnectMsg(FALSE);		
		ExitClient();
		return CLIENT_SETUP_FAIL;		
	}
	
	//创建发送和接收数据线程
	if (!CreateSendAndRecvThread())
	{
		ExitClient();
		return CLIENT_CREATETHREAD_FAIL;
	}
	
	//用户输入数据和显示结果
	InputAndOutput();
	
	//退出
	ExitClient();
	
	return 0;
}

/**
 *	初始化
 */
BOOL	InitClient(void)
{
	//初始化全局变量
	InitMember();

	//创建SOCKET
	if (!InitSockt())
	{
		return FALSE;
	}

	return TRUE;	
}


/**
 * 初始化全局变量
 */
void	InitMember(void)
{
	//初始化临界区
	InitializeCriticalSection(&csSend);
	InitializeCriticalSection(&csRecv);

	sClient = INVALID_SOCKET;	//套接字
	hThreadRecv = NULL;			//接收数据线程句柄
	hThreadSend = NULL;			//发送数据线程句柄
	bConnecting = FALSE;		//为连接状态
	bSendData = FALSE;			//不发送数据状态	

	//初始化数据缓冲区
	memset(bufSend.buf, 0, MAX_NUM_BUF);
	memset(bufRecv.buf, 0, MAX_NUM_BUF);
	memset(arrThread, 0, 2);
	
	//手动设置事件，初始化为无信号状态
	hEventShowDataResult = (HANDLE)CreateEvent(NULL, TRUE, FALSE, NULL);
}	
				
/**
 * 创建非阻塞套接字
 */
BOOL    InitSockt(void)
{
	int			reVal;	//返回值
	WSADATA		wsData;	//WSADATA变量	
	reVal = WSAStartup(MAKEWORD(2,2),&wsData);//初始化Windows Sockets Dll
	
	//创建套接字		
	sClient = socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == sClient)
		return FALSE;

	
	//设置套接字非阻塞模式
	unsigned long ul = 1;
	reVal = ioctlsocket(sClient, FIONBIO, (unsigned long*)&ul);
	if (reVal == SOCKET_ERROR)
		return FALSE;

	return TRUE;
}	
			
/**
 * 客户端退出
 */
void	ExitClient(void)			
{
	DeleteCriticalSection(&csSend);
	DeleteCriticalSection(&csRecv);
	CloseHandle(hThreadRecv);
	CloseHandle(hThreadSend);
	closesocket(sClient);
	WSACleanup();
	return;
}

/**
 * 连接服务器
 */
BOOL	ConnectServer(void)
{
	int reVal;			//返回值
	sockaddr_in serAddr;//服务器地址
	
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(SERVERPORT);
	serAddr.sin_addr.S_un.S_addr = inet_addr(SERVERIP);

	for (;;)
	{
		//连接服务器
		reVal = connect(sClient, (struct sockaddr*)&serAddr, sizeof(serAddr));
		
		//处理连接错误
		if(SOCKET_ERROR == reVal)
		{
			int nErrCode = WSAGetLastError();
			if( WSAEWOULDBLOCK == nErrCode ||//连接还没有完成
					 WSAEINVAL == nErrCode)
			{
				continue;
			}else if (WSAEISCONN == nErrCode)//连接已经完成
			{
				break;
			}else//其它原因，连接失败
			{
				return FALSE;
			}
		}	
		
		if ( reVal == 0 )//连接成功
			break;		
	}

	bConnecting = TRUE;

	return TRUE;
}

/**
 * 显示连接服务器失败信息
 */
void	ShowConnectMsg(BOOL bSuc)
{
	if (bSuc)
	{
		cout << "******************************" << endl;
		cout << "*                            *" << endl;
		cout << "* Succeed to connect server! *" << endl;
		cout << "*                            *" << endl;
		cout << "******************************" << endl;
	}else{
		cout << "***************************" << endl;
		cout << "*                         *" << endl;
		cout << "* Fail to connect server! *" << endl;
		cout << "*                         *" << endl;
		cout << "***************************" << endl;
	}
	
    return;
}

/**
 * 表达式结果
 */
void	ShowDataResultMsg(void)
{
	EnterCriticalSection(&csRecv);
	cout << "**********************************" << endl;
	cout << "*                                *" << endl;
	cout << "*  Result:                       *" << endl;
	cout <<  bufRecv.buf <<endl;
	cout << "*                                *" << endl;
	cout << "**********************************" << endl;	
	LeaveCriticalSection(&csRecv);
		
}

/**
 * 提示信息
 */
void	ShowTipMsg(BOOL bFirstInput)
{
	if (bFirstInput)//首次显示
	{
		cout << "**********************************" << endl;
		cout << "*                                *" << endl;
		cout << "* Please input expression.       *" << endl;
		cout << "* Usage:NumberOperatorNumber=    *" << endl;
		cout << "*                                *" << endl;
		cout << "**********************************" << endl;
	}else{
		cout << "**********************************" << endl;
		cout << "*                                *" << endl;
		cout << "* Please input: expression       *" << endl;
		cout << "* Usage:NumberOperatorNumber=    *" << endl;
		cout << "*                                *" << endl;
		cout << "* If you want to exit.           *" << endl;
		cout << "* Usage: Byebye or byebye        *" << endl;
		cout << "*                                *" << endl;
		cout << "**********************************" << endl;
	}	
	
}
/**
 * 创建发送和接收数据线程
 */
BOOL	CreateSendAndRecvThread(void)
{
	//创建接收数据的线程
	unsigned long ulThreadId;
	hThreadRecv = CreateThread(NULL, 0, RecvDataThread, NULL, 0, &ulThreadId);
	if (NULL == hThreadRecv)
		return FALSE;
	
	//创建发送数据的线程
	hThreadSend = CreateThread(NULL, 0, SendDataThread, NULL, 0, &ulThreadId);
	if (NULL == hThreadSend)
		return FALSE;

	//添加到线程数组
	arrThread[0] = hThreadRecv;
	arrThread[1] = hThreadSend;	
	return TRUE;
}

/**
 * 输入数据和显示结果
 */
void	InputAndOutput(void)
{
	char cInput[MAX_NUM_BUF];	//用户输入缓冲区	
	BOOL bFirstInput = TRUE;	//第一次只能输入算数表达式
	
	for (;bConnecting;)			//连接状态
	{
		memset(cInput, 0, MAX_NUM_BUF);		
		
		ShowTipMsg(bFirstInput);		//提示输入信息
		
		cin >> cInput;					//输入表达式		
		char *pTemp = cInput;
		if (bFirstInput)				//第一次输入
		{
			if (!PackExpression(pTemp))	//算数表达式打包
			{
				continue;				//重新输入
			}
			bFirstInput = FALSE;		//成功输入第一个算数表达式
			
		}else if (!PackByebye(pTemp))	//“Byebye”“byebye”打包
		{			
			if (!PackExpression(pTemp))	//算数表达式打包
			{
				continue;				//重新输入
			}
		}
		
		//等待显示计算结果
		if (WAIT_OBJECT_0 == WaitForSingleObject(hEventShowDataResult, INFINITE))
		{
			ResetEvent(hEventShowDataResult);	//设置为无信号状态				
			if (!bConnecting)					//客户端被动退出，此时接收和发送数据线程已经退出。					
			{
				break;
			}
			
			ShowDataResultMsg();				//显示数据

			if (0 == strcmp(bufRecv.buf, "OK"))	//客户端主动退出
			{
				bConnecting = FALSE;
				Sleep(TIMEFOR_THREAD_EXIT);		//给数据接收和发送线程退出时间
			}			
		}	
	}

	if (!bConnecting)			//与服务器连接已经断开
	{
		ShowConnectMsg(FALSE);	//显示信息
	}

	//等待数据发送和接收线程退出
	DWORD reVal = WaitForMultipleObjects(2, arrThread, TRUE, INFINITE);
	if (WAIT_ABANDONED_0 == reVal)
	{
		int nErrCode = GetLastError();
	}

}

/**
 * 接收数据线程
 */
DWORD __stdcall	RecvDataThread(void* pParam)				
{
	int		reVal;				//返回值
	char	temp[MAX_NUM_BUF];	//局部变量
	memset(temp, 0, MAX_NUM_BUF);

	while(bConnecting)			//连接状态		
	{	

		reVal = recv(sClient, temp, MAX_NUM_BUF, 0);//接收数据
		
		if (SOCKET_ERROR == reVal)
		{
			int nErrCode = WSAGetLastError();
			if (WSAEWOULDBLOCK == nErrCode)			//接受数据缓冲区不可用
			{
				Sleep(TIMEFOR_THREAD_SLEEP);		//线程睡眠
				continue;							//继续接收数据
			}else{
				bConnecting = FALSE;
				SetEvent(hEventShowDataResult);		//通知主线程，防止在无限期的等待
				return 0;							//线程退出				
			}
		}
		
		if ( reVal == 0)							//服务器关闭了连接
		{
			bConnecting = FALSE;					//线程退出
			SetEvent(hEventShowDataResult);			//通知主线程，防止在无限期的等待
			return 0;								//线程退出 
		}
		
		if (reVal > HEADERLEN && -1 != reVal)		//收到数据
		{
			//对数据解包
 			phdr header = (phdr)(temp);
			EnterCriticalSection(&csRecv);		
			memset(bufRecv.buf, 0, MAX_NUM_BUF);	
			memcpy(bufRecv.buf, temp + HEADERLEN, header->len - HEADERLEN);	//将数据结果复制到接收数据缓冲区
			LeaveCriticalSection(&csRecv);

			SetEvent(hEventShowDataResult);									//通知主线程显示计算结果
			memset(temp, 0, MAX_NUM_BUF);
		}
		
		Sleep(TIMEFOR_THREAD_SLEEP);//线程睡眠
	}
	return 0;
}

/**
 * 发送数据线程
 */
DWORD __stdcall	SendDataThread(void* pParam)			
{	
	while(bConnecting)						//连接状态
	{
		if (bSendData)						//发送数据
		{
			EnterCriticalSection(&csSend);	//进入临界区
			for (;;)
			{
				int nBuflen = ((phdr)(bufSend.buf))->len;		
				int val = send(sClient, bufSend.buf, nBuflen,0);
				
				//处理返回错误
				if (SOCKET_ERROR == val)
				{
					int nErrCode = WSAGetLastError();
					if(WSAEWOULDBLOCK == nErrCode)		//发送缓冲区不可用
					{
						continue;						//继续循环
					}else {
						LeaveCriticalSection(&csSend);	//离开临界区
						bConnecting = FALSE;			//断开状态
						SetEvent(hEventShowDataResult);	//通知主线程，防止在无限期的等待返回结果。
						return 0;
					}				
				}				
			
				bSendData = FALSE;			//发送状态
				break;						//跳出for
			}			
			LeaveCriticalSection(&csSend);	//离开临界区
		}
			
		Sleep(TIMEFOR_THREAD_SLEEP);		//线程睡眠
	}
	
	return 0;
}

/**
 * 打包发送byebye数据
 */
BOOL	PackByebye(const char* pExpr)
{
	BOOL reVal = FALSE;
	
	if(!strcmp("Byebye", pExpr)||!strcmp("byebye", pExpr))		//如果是"Byebye" "byebye"
	{
		EnterCriticalSection(&csSend);							//进入临界区
		phdr pHeader = (phdr)bufSend.buf;						//强制转换
		pHeader->type = BYEBYE;									//类型
		pHeader->len = HEADERLEN + strlen("Byebye");			//数据包长度
		memcpy(bufSend.buf + HEADERLEN, pExpr, strlen(pExpr));	//复制数据
		LeaveCriticalSection(&csSend);							//离开临界区		
		
		pHeader = NULL;											//null
		bSendData = TRUE;										//通知发送数据线程
		reVal = TRUE;		
	}
	
	return reVal;
}

/**
 * 打包计算表达式的数据
 */
BOOL	PackExpression(const char *pExpr)
{
	
	char* pTemp = (char*)pExpr;	//算数表达式数字开始的位置
	while (!*pTemp)				//第一个数字位置
		pTemp++;		
	
	char* pos1 = pTemp;	//第一个数字位置
	char* pos2 = NULL;	//运算符位置
	char* pos3 = NULL;	//第二个数字位置
	int len1 = 0;		//第一个数字长度
	int len2 = 0;		//运算符长度
	int len3 = 0;		//第二个数字长度

	//第一个字符是+ - 或者是数字
	if ((*pTemp != '+') && 
		(*pTemp != '-') &&
		((*pTemp < '0') || (*pTemp > '9')))
	{
		return FALSE;
	}

	
	if ((*pTemp++ == '+')&&(*pTemp < '0' || *pTemp > '9'))	//第一个字符是'+'，第二个是数字	
		return FALSE;										//重新输入
	--pTemp;												//上移指针
	
	
	if ((*pTemp++ == '-')&&(*pTemp < '0' || *pTemp > '9'))	//第一个字符是'-',第二个是数字	
		return FALSE;										//重新输入
	--pTemp;												//上移指针
	
	char* pNum = pTemp;						//数字开始的位置					
	if (*pTemp == '+'||*pTemp == '-')		//+ -
		pTemp++;
	
	while (*pTemp >= '0' && *pTemp <= '9')	//数字
		pTemp++;							
	
	len1 = pTemp - pNum;//数字长度						
	
	//可能有空格
	while(!*pTemp)							
		pTemp++;
	
	//算数运算符
	if ((ADD != *pTemp)&&			
		(SUB != *pTemp)&&
		(MUT != *pTemp)&&
		(DIV != *pTemp))
		return FALSE;
	
	pos2 = pTemp;
	len2 = 1;
	
	//下移指针
	pTemp++;
	//可能有空格
	while(!*pTemp)
		pTemp++;
	
	//第2个数字位置
	pos3 = pTemp;
	if (*pTemp < '0' || *pTemp > '9')
		return FALSE;//重新输入			
	
	while (*pTemp >= '0' && *pTemp <= '9')//数字
		pTemp++;
	
	if (EQU != *pTemp)	//最后是等于号
		return FALSE;	//重新输入
	
	len3 = pTemp - pos3;//数字长度
	

	int nExprlen = len1 + len2 + len3;	//算数表示长度

	//表达式读入发送数据缓冲区
	EnterCriticalSection(&csSend);		//进入临界区
	//数据包头
	phdr pHeader = (phdr)(bufSend.buf);
	pHeader->type = EXPRESSION;			//类型
	pHeader->len = nExprlen + HEADERLEN;//数据包长度
	//拷贝数据
	memcpy(bufSend.buf + HEADERLEN, pos1, len1);
	memcpy(bufSend.buf + HEADERLEN + len1, pos2, len2);
	memcpy(bufSend.buf + HEADERLEN + len1 + len2 , pos3,len3);
	LeaveCriticalSection(&csSend);		//离开临界区
	pHeader = NULL;

	bSendData = TRUE;					//通知发送数据线程发送数据

	return TRUE;
}



