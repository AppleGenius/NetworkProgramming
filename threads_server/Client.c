#include "StdAfx.h"
#include <process.h>
#include "Client.h"

/*
 * 构造函数
 */
CClient::CClient(const SOCKET sClient, const sockaddr_in &addrClient)
{
	//初始化变量
	m_hThreadRecv = NULL;
	m_hThreadSend = NULL;
	m_socket = sClient;
	m_addr = addrClient;
	m_bConning = FALSE;	
	m_bExit = FALSE;
	memset(m_data.buf, 0, MAX_NUM_BUF);

	//创建事件
	m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);//手动设置信号状态，初始化为无信号状态

	//初始化临界区
	InitializeCriticalSection(&m_cs);
}
/*
 * 析构函数
 */
CClient::~CClient()
{
	closesocket(m_socket);			//关闭套接字
	m_socket = INVALID_SOCKET;		//套接字无效
	DeleteCriticalSection(&m_cs);	//释放临界区对象	
	CloseHandle(m_hEvent);			//释放事件对象
}

/*
 * 创建发送和接收数据线程
 */
BOOL CClient::StartRuning(void)
{
	m_bConning = TRUE;//设置连接状态

	//创建接收数据线程
	unsigned long ulThreadId;
	m_hThreadRecv = CreateThread(NULL, 0, RecvDataThread, this, 0, &ulThreadId);
	if(NULL == m_hThreadRecv)
	{
		return FALSE;
	}else{
		CloseHandle(m_hThreadRecv);
	}

	//创建接收客户端数据的线程
	m_hThreadSend =  CreateThread(NULL, 0, SendDataThread, this, 0, &ulThreadId);
	if(NULL == m_hThreadSend)
	{
		return FALSE;
	}else{
		CloseHandle(m_hThreadSend);
	}
	
	return TRUE;
}


/*
 * 接收客户端数据
 */
DWORD  CClient::RecvDataThread(void* pParam)
{
	CClient *pClient = (CClient*)pParam;	//客户端对象指针
	int		reVal;							//返回值
	char	temp[MAX_NUM_BUF];				//临时变量

	memset(temp, 0, MAX_NUM_BUF);
	
	for (;pClient->m_bConning;)				//连接状态
	{
		reVal = recv(pClient->m_socket, temp, MAX_NUM_BUF, 0);	//接收数据
		
		//处理错误返回值
		if (SOCKET_ERROR == reVal)
		{
			int nErrCode = WSAGetLastError();
			
			if ( WSAEWOULDBLOCK == nErrCode )	//接受数据缓冲区不可用
			{
				continue;						//继续循环
			}else if (WSAENETDOWN == nErrCode ||//客户端关闭了连接
					 WSAETIMEDOUT == nErrCode ||
					WSAECONNRESET == nErrCode )			
			{
				break;							//线程退出				
			}
		}
		
		//客户端关闭了连接
		if ( reVal == 0)	
		{
			break;
		}
	
		//收到数据
		if (reVal > HEADERLEN)			
		{		
			pClient->HandleData(temp);		//处理数据

			SetEvent(pClient->m_hEvent);	//通知发送数据线程

			memset(temp, 0, MAX_NUM_BUF);	//清空临时变量
		}
		
		Sleep(TIMEFOR_THREAD_CLIENT);		//线程睡眠
	}
	
	pClient->m_bConning = FALSE;			//与客户端的连接断开
	SetEvent(pClient->m_hEvent);			//通知发送数据线程退出
	
	return 0;								//线程退出
}

/*
 * //向客户端发送数据
 */
DWORD CClient::SendDataThread(void* pParam)	
{
	CClient *pClient = (CClient*)pParam;//转换数据类型为CClient指针

	for (;pClient->m_bConning;)//连接状态
	{	
		//收到事件通知
		if (WAIT_OBJECT_0 == WaitForSingleObject(pClient->m_hEvent, INFINITE))
		{
			//当客户端的连接断开时，接收数据线程先退出，然后该线程后退出，并设置退出标志
			if (!pClient->m_bConning)
			{
				pClient->m_bExit = TRUE;
				break ;
			}

			//进入临界区
			EnterCriticalSection(&pClient->m_cs);		
			//发送数据
			phdr pHeader = (phdr)pClient->m_data.buf;
			int nSendlen = pHeader->len;

			int val = send(pClient->m_socket, pClient->m_data.buf, nSendlen,0);
			//处理返回错误
			if (SOCKET_ERROR == val)
			{
				int nErrCode = WSAGetLastError();
				if (nErrCode == WSAEWOULDBLOCK)//发送数据缓冲区不可用
				{
					continue;
				}else if ( WSAENETDOWN == nErrCode || 
						  WSAETIMEDOUT == nErrCode ||
						  WSAECONNRESET == nErrCode)//客户端关闭了连接
				{
					//离开临界区
					LeaveCriticalSection(&pClient->m_cs);
					pClient->m_bConning = FALSE;	//连接断开
					pClient->m_bExit = TRUE;		//线程退出
					break;			
				}else {
					//离开临界区
					LeaveCriticalSection(&pClient->m_cs);
					pClient->m_bConning = FALSE;	//连接断开
					pClient->m_bExit = TRUE;		//线程退出
					break;
				}				
			}
			//成功发送数据
			//离开临界区
			LeaveCriticalSection(&pClient->m_cs);
			//设置事件为无信号状态
			ResetEvent(&pClient->m_hEvent);	
		}	
		
	}

	return 0;
}
/*
 *  计算表达式,打包数据
 */
void CClient::HandleData(const char* pExpr)	
{
	memset(m_data.buf, 0, MAX_NUM_BUF);//清空m_data
	
    //如果是“byebye”或者“Byebye”
	if (BYEBYE == ((phdr)pExpr)->type)		
	{
		EnterCriticalSection(&m_cs);
		phdr pHeaderSend = (phdr)m_data.buf;				//发送的数据		
		pHeaderSend->type = BYEBYE;							//单词类型
		pHeaderSend->len = HEADERLEN + strlen("OK");		//数据包长度
		memcpy(m_data.buf + HEADERLEN, "OK", strlen("OK"));	//复制数据到m_data"
		LeaveCriticalSection(&m_cs);
		
	}else{//算数表达式
		
		int nFirNum;		//第一个数字
		int nSecNum;		//第二个数字
		char cOper;			//算数运算符
		int nResult;		//计算结果
		//格式化读入数据
		sscanf(pExpr + HEADERLEN, "%d%c%d", &nFirNum, &cOper, &nSecNum);
		
		//计算
		switch(cOper)
		{
		case '+'://加
			{
				nResult = nFirNum + nSecNum;
				break;
			}
		case '-'://减
			{
				nResult = nFirNum - nSecNum;
				break;
			}
		case '*'://乘
			{
				nResult = nFirNum * nSecNum;				
				break;
			}
		case '/'://除
			{
				if (ZERO == nSecNum)//无效的数字
				{
					nResult = INVALID_NUM;
				}else
				{
					nResult = nFirNum / nSecNum;	
				}				
				break;
			}
		default:
			nResult = INVALID_OPERATOR;//无效操作符
			break;			
		}
		
		//将算数表达式和计算的结果写入字符数组中
		char temp[MAX_NUM_BUF];
		char cEqu = '=';
		sprintf(temp, "%d%c%d%c%d",nFirNum, cOper, nSecNum,cEqu, nResult);

		//打包数据
		EnterCriticalSection(&m_cs);
		phdr pHeaderSend = (phdr)m_data.buf;				//发送的数据		
		pHeaderSend->type = EXPRESSION;						//数据类型为算数表达式
		pHeaderSend->len = HEADERLEN + strlen(temp);		//数据包的长度
		memcpy(m_data.buf + HEADERLEN, temp, strlen(temp));	//复制数据到m_data
		LeaveCriticalSection(&m_cs);
		
	}
}

