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


#define CLIENT_SETUP_FAIL			1		//�����ͻ���ʧ��
#define CLIENT_CREATETHREAD_FAIL	2		//�����߳�ʧ��
#define CLIENT_CONNECT_FAIL			3		//�������ӶϿ�
#define TIMEFOR_THREAD_EXIT			1000	//���߳��˳�ʱ��
#define TIMEFOR_THREAD_SLEEP		500		//�߳�˯��ʱ��

#define	SERVERIP			"127.0.0.1"		//������IP
#define	SERVERPORT			5556			//������TCP�˿�
#define	MAX_NUM_BUF			48				//����������󳤶�
#define ADD					'+'				//+
#define SUB					'-'				//- 
#define MUT					'*'				//*
#define DIV					'/'				///
#define EQU					'='				//=

//���ݰ�����
#define EXPRESSION			'E'				//�������ʽ
#define BYEBYE				'B'				//��Ϣbyebye
#define HEADERLEN			(sizeof(hdr))	//ͷ����

//���ݰ�ͷ�ṹ�ýṹ��win32xp��Ϊ4byte
typedef struct _head
{
	char			type;//����		
	unsigned short	len;//���ݰ��ĳ���(����ͷ�ĳ���)
}hdr, *phdr;

//���ݰ��е����ݽṹ
typedef struct _data 
{
	char	buf[MAX_NUM_BUF];
}DATABUF, *pDataBuf;

SOCKET	sClient;							//�׽���
HANDLE	hThreadSend;						//���������߳�
HANDLE	hThreadRecv;						//���������߳�
DATABUF bufSend;							//�������ݻ�����
DATABUF bufRecv;							//�������ݻ�����
CRITICAL_SECTION csSend;					//�ٽ�����������bufSend
CRITICAL_SECTION csRecv;					//�ٽ�����������bufRecv
BOOL	bSendData;							//֪ͨ���������߳�
HANDLE	hEventShowDataResult;				//��ʾ���������¼�
BOOL	bConnecting;						//�������������״̬
HANDLE	arrThread[2];						//���߳�����


BOOL	InitClient(void);					//��ʼ��
BOOL	ConnectServer(void);				//���ӷ�����
BOOL	CreateSendAndRecvThread(void);		//�������ͺͽ��������߳�
void	InputAndOutput(void);				//�û���������
void	ExitClient(void);					//�˳�

void	InitMember(void);					//��ʼ��ȫ�ֱ���
BOOL    InitSockt(void);					//����SOCKET

DWORD __stdcall	RecvDataThread(void* pParam);		//���������߳�
DWORD __stdcall	SendDataThread(void* pParam);		//���������߳�

BOOL	PackByebye(const char* pExpr);		//�������"Byebye" "byebye"���ַ������
BOOL	PackExpression(const char *pExpr);	//��������������ʽ���

void	ShowConnectMsg(BOOL bSuc);			//��ʾ���ӷ�������Ϣ
void	ShowDataResultMsg(void);			//��ʾ��������
void	ShowTipMsg(BOOL bFirstInput);		//��ʾ��ʾ��Ϣ


int main(int argc, char* argv[])
{	
	//��ʼ��
	if (!InitClient())
	{	
		ExitClient();
		return CLIENT_SETUP_FAIL;
	}	
	
	//���ӷ�����
	if (ConnectServer())
	{
		ShowConnectMsg(TRUE);	
	}else{
		ShowConnectMsg(FALSE);		
		ExitClient();
		return CLIENT_SETUP_FAIL;		
	}
	
	//�������ͺͽ��������߳�
	if (!CreateSendAndRecvThread())
	{
		ExitClient();
		return CLIENT_CREATETHREAD_FAIL;
	}
	
	//�û��������ݺ���ʾ���
	InputAndOutput();
	
	//�˳�
	ExitClient();
	
	return 0;
}

/**
 *	��ʼ��
 */
BOOL	InitClient(void)
{
	//��ʼ��ȫ�ֱ���
	InitMember();

	//����SOCKET
	if (!InitSockt())
	{
		return FALSE;
	}

	return TRUE;	
}


/**
 * ��ʼ��ȫ�ֱ���
 */
void	InitMember(void)
{
	//��ʼ���ٽ���
	InitializeCriticalSection(&csSend);
	InitializeCriticalSection(&csRecv);

	sClient = INVALID_SOCKET;	//�׽���
	hThreadRecv = NULL;			//���������߳̾��
	hThreadSend = NULL;			//���������߳̾��
	bConnecting = FALSE;		//Ϊ����״̬
	bSendData = FALSE;			//����������״̬	

	//��ʼ�����ݻ�����
	memset(bufSend.buf, 0, MAX_NUM_BUF);
	memset(bufRecv.buf, 0, MAX_NUM_BUF);
	memset(arrThread, 0, 2);
	
	//�ֶ������¼�����ʼ��Ϊ���ź�״̬
	hEventShowDataResult = (HANDLE)CreateEvent(NULL, TRUE, FALSE, NULL);
}	
				
/**
 * �����������׽���
 */
BOOL    InitSockt(void)
{
	int			reVal;	//����ֵ
	WSADATA		wsData;	//WSADATA����	
	reVal = WSAStartup(MAKEWORD(2,2),&wsData);//��ʼ��Windows Sockets Dll
	
	//�����׽���		
	sClient = socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == sClient)
		return FALSE;

	
	//�����׽��ַ�����ģʽ
	unsigned long ul = 1;
	reVal = ioctlsocket(sClient, FIONBIO, (unsigned long*)&ul);
	if (reVal == SOCKET_ERROR)
		return FALSE;

	return TRUE;
}	
			
/**
 * �ͻ����˳�
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
 * ���ӷ�����
 */
BOOL	ConnectServer(void)
{
	int reVal;			//����ֵ
	sockaddr_in serAddr;//��������ַ
	
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(SERVERPORT);
	serAddr.sin_addr.S_un.S_addr = inet_addr(SERVERIP);

	for (;;)
	{
		//���ӷ�����
		reVal = connect(sClient, (struct sockaddr*)&serAddr, sizeof(serAddr));
		
		//�������Ӵ���
		if(SOCKET_ERROR == reVal)
		{
			int nErrCode = WSAGetLastError();
			if( WSAEWOULDBLOCK == nErrCode ||//���ӻ�û�����
					 WSAEINVAL == nErrCode)
			{
				continue;
			}else if (WSAEISCONN == nErrCode)//�����Ѿ����
			{
				break;
			}else//����ԭ������ʧ��
			{
				return FALSE;
			}
		}	
		
		if ( reVal == 0 )//���ӳɹ�
			break;		
	}

	bConnecting = TRUE;

	return TRUE;
}

/**
 * ��ʾ���ӷ�����ʧ����Ϣ
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
 * ���ʽ���
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
 * ��ʾ��Ϣ
 */
void	ShowTipMsg(BOOL bFirstInput)
{
	if (bFirstInput)//�״���ʾ
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
 * �������ͺͽ��������߳�
 */
BOOL	CreateSendAndRecvThread(void)
{
	//�����������ݵ��߳�
	unsigned long ulThreadId;
	hThreadRecv = CreateThread(NULL, 0, RecvDataThread, NULL, 0, &ulThreadId);
	if (NULL == hThreadRecv)
		return FALSE;
	
	//�����������ݵ��߳�
	hThreadSend = CreateThread(NULL, 0, SendDataThread, NULL, 0, &ulThreadId);
	if (NULL == hThreadSend)
		return FALSE;

	//��ӵ��߳�����
	arrThread[0] = hThreadRecv;
	arrThread[1] = hThreadSend;	
	return TRUE;
}

/**
 * �������ݺ���ʾ���
 */
void	InputAndOutput(void)
{
	char cInput[MAX_NUM_BUF];	//�û����뻺����	
	BOOL bFirstInput = TRUE;	//��һ��ֻ�������������ʽ
	
	for (;bConnecting;)			//����״̬
	{
		memset(cInput, 0, MAX_NUM_BUF);		
		
		ShowTipMsg(bFirstInput);		//��ʾ������Ϣ
		
		cin >> cInput;					//������ʽ		
		char *pTemp = cInput;
		if (bFirstInput)				//��һ������
		{
			if (!PackExpression(pTemp))	//�������ʽ���
			{
				continue;				//��������
			}
			bFirstInput = FALSE;		//�ɹ������һ���������ʽ
			
		}else if (!PackByebye(pTemp))	//��Byebye����byebye�����
		{			
			if (!PackExpression(pTemp))	//�������ʽ���
			{
				continue;				//��������
			}
		}
		
		//�ȴ���ʾ������
		if (WAIT_OBJECT_0 == WaitForSingleObject(hEventShowDataResult, INFINITE))
		{
			ResetEvent(hEventShowDataResult);	//����Ϊ���ź�״̬				
			if (!bConnecting)					//�ͻ��˱����˳�����ʱ���պͷ��������߳��Ѿ��˳���					
			{
				break;
			}
			
			ShowDataResultMsg();				//��ʾ����

			if (0 == strcmp(bufRecv.buf, "OK"))	//�ͻ��������˳�
			{
				bConnecting = FALSE;
				Sleep(TIMEFOR_THREAD_EXIT);		//�����ݽ��պͷ����߳��˳�ʱ��
			}			
		}	
	}

	if (!bConnecting)			//������������Ѿ��Ͽ�
	{
		ShowConnectMsg(FALSE);	//��ʾ��Ϣ
	}

	//�ȴ����ݷ��ͺͽ����߳��˳�
	DWORD reVal = WaitForMultipleObjects(2, arrThread, TRUE, INFINITE);
	if (WAIT_ABANDONED_0 == reVal)
	{
		int nErrCode = GetLastError();
	}

}

/**
 * ���������߳�
 */
DWORD __stdcall	RecvDataThread(void* pParam)				
{
	int		reVal;				//����ֵ
	char	temp[MAX_NUM_BUF];	//�ֲ�����
	memset(temp, 0, MAX_NUM_BUF);

	while(bConnecting)			//����״̬		
	{	

		reVal = recv(sClient, temp, MAX_NUM_BUF, 0);//��������
		
		if (SOCKET_ERROR == reVal)
		{
			int nErrCode = WSAGetLastError();
			if (WSAEWOULDBLOCK == nErrCode)			//�������ݻ�����������
			{
				Sleep(TIMEFOR_THREAD_SLEEP);		//�߳�˯��
				continue;							//������������
			}else{
				bConnecting = FALSE;
				SetEvent(hEventShowDataResult);		//֪ͨ���̣߳���ֹ�������ڵĵȴ�
				return 0;							//�߳��˳�				
			}
		}
		
		if ( reVal == 0)							//�������ر�������
		{
			bConnecting = FALSE;					//�߳��˳�
			SetEvent(hEventShowDataResult);			//֪ͨ���̣߳���ֹ�������ڵĵȴ�
			return 0;								//�߳��˳� 
		}
		
		if (reVal > HEADERLEN && -1 != reVal)		//�յ�����
		{
			//�����ݽ��
 			phdr header = (phdr)(temp);
			EnterCriticalSection(&csRecv);		
			memset(bufRecv.buf, 0, MAX_NUM_BUF);	
			memcpy(bufRecv.buf, temp + HEADERLEN, header->len - HEADERLEN);	//�����ݽ�����Ƶ��������ݻ�����
			LeaveCriticalSection(&csRecv);

			SetEvent(hEventShowDataResult);									//֪ͨ���߳���ʾ������
			memset(temp, 0, MAX_NUM_BUF);
		}
		
		Sleep(TIMEFOR_THREAD_SLEEP);//�߳�˯��
	}
	return 0;
}

/**
 * ���������߳�
 */
DWORD __stdcall	SendDataThread(void* pParam)			
{	
	while(bConnecting)						//����״̬
	{
		if (bSendData)						//��������
		{
			EnterCriticalSection(&csSend);	//�����ٽ���
			for (;;)
			{
				int nBuflen = ((phdr)(bufSend.buf))->len;		
				int val = send(sClient, bufSend.buf, nBuflen,0);
				
				//�����ش���
				if (SOCKET_ERROR == val)
				{
					int nErrCode = WSAGetLastError();
					if(WSAEWOULDBLOCK == nErrCode)		//���ͻ�����������
					{
						continue;						//����ѭ��
					}else {
						LeaveCriticalSection(&csSend);	//�뿪�ٽ���
						bConnecting = FALSE;			//�Ͽ�״̬
						SetEvent(hEventShowDataResult);	//֪ͨ���̣߳���ֹ�������ڵĵȴ����ؽ����
						return 0;
					}				
				}				
			
				bSendData = FALSE;			//����״̬
				break;						//����for
			}			
			LeaveCriticalSection(&csSend);	//�뿪�ٽ���
		}
			
		Sleep(TIMEFOR_THREAD_SLEEP);		//�߳�˯��
	}
	
	return 0;
}

/**
 * �������byebye����
 */
BOOL	PackByebye(const char* pExpr)
{
	BOOL reVal = FALSE;
	
	if(!strcmp("Byebye", pExpr)||!strcmp("byebye", pExpr))		//�����"Byebye" "byebye"
	{
		EnterCriticalSection(&csSend);							//�����ٽ���
		phdr pHeader = (phdr)bufSend.buf;						//ǿ��ת��
		pHeader->type = BYEBYE;									//����
		pHeader->len = HEADERLEN + strlen("Byebye");			//���ݰ�����
		memcpy(bufSend.buf + HEADERLEN, pExpr, strlen(pExpr));	//��������
		LeaveCriticalSection(&csSend);							//�뿪�ٽ���		
		
		pHeader = NULL;											//null
		bSendData = TRUE;										//֪ͨ���������߳�
		reVal = TRUE;		
	}
	
	return reVal;
}

/**
 * ���������ʽ������
 */
BOOL	PackExpression(const char *pExpr)
{
	
	char* pTemp = (char*)pExpr;	//�������ʽ���ֿ�ʼ��λ��
	while (!*pTemp)				//��һ������λ��
		pTemp++;		
	
	char* pos1 = pTemp;	//��һ������λ��
	char* pos2 = NULL;	//�����λ��
	char* pos3 = NULL;	//�ڶ�������λ��
	int len1 = 0;		//��һ�����ֳ���
	int len2 = 0;		//���������
	int len3 = 0;		//�ڶ������ֳ���

	//��һ���ַ���+ - ����������
	if ((*pTemp != '+') && 
		(*pTemp != '-') &&
		((*pTemp < '0') || (*pTemp > '9')))
	{
		return FALSE;
	}

	
	if ((*pTemp++ == '+')&&(*pTemp < '0' || *pTemp > '9'))	//��һ���ַ���'+'���ڶ���������	
		return FALSE;										//��������
	--pTemp;												//����ָ��
	
	
	if ((*pTemp++ == '-')&&(*pTemp < '0' || *pTemp > '9'))	//��һ���ַ���'-',�ڶ���������	
		return FALSE;										//��������
	--pTemp;												//����ָ��
	
	char* pNum = pTemp;						//���ֿ�ʼ��λ��					
	if (*pTemp == '+'||*pTemp == '-')		//+ -
		pTemp++;
	
	while (*pTemp >= '0' && *pTemp <= '9')	//����
		pTemp++;							
	
	len1 = pTemp - pNum;//���ֳ���						
	
	//�����пո�
	while(!*pTemp)							
		pTemp++;
	
	//���������
	if ((ADD != *pTemp)&&			
		(SUB != *pTemp)&&
		(MUT != *pTemp)&&
		(DIV != *pTemp))
		return FALSE;
	
	pos2 = pTemp;
	len2 = 1;
	
	//����ָ��
	pTemp++;
	//�����пո�
	while(!*pTemp)
		pTemp++;
	
	//��2������λ��
	pos3 = pTemp;
	if (*pTemp < '0' || *pTemp > '9')
		return FALSE;//��������			
	
	while (*pTemp >= '0' && *pTemp <= '9')//����
		pTemp++;
	
	if (EQU != *pTemp)	//����ǵ��ں�
		return FALSE;	//��������
	
	len3 = pTemp - pos3;//���ֳ���
	

	int nExprlen = len1 + len2 + len3;	//������ʾ����

	//���ʽ���뷢�����ݻ�����
	EnterCriticalSection(&csSend);		//�����ٽ���
	//���ݰ�ͷ
	phdr pHeader = (phdr)(bufSend.buf);
	pHeader->type = EXPRESSION;			//����
	pHeader->len = nExprlen + HEADERLEN;//���ݰ�����
	//��������
	memcpy(bufSend.buf + HEADERLEN, pos1, len1);
	memcpy(bufSend.buf + HEADERLEN + len1, pos2, len2);
	memcpy(bufSend.buf + HEADERLEN + len1 + len2 , pos3,len3);
	LeaveCriticalSection(&csSend);		//�뿪�ٽ���
	pHeader = NULL;

	bSendData = TRUE;					//֪ͨ���������̷߳�������

	return TRUE;
}



