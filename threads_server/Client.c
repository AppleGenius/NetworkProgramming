#include "StdAfx.h"
#include <process.h>
#include "Client.h"

/*
 * ���캯��
 */
CClient::CClient(const SOCKET sClient, const sockaddr_in &addrClient)
{
	//��ʼ������
	m_hThreadRecv = NULL;
	m_hThreadSend = NULL;
	m_socket = sClient;
	m_addr = addrClient;
	m_bConning = FALSE;	
	m_bExit = FALSE;
	memset(m_data.buf, 0, MAX_NUM_BUF);

	//�����¼�
	m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);//�ֶ������ź�״̬����ʼ��Ϊ���ź�״̬

	//��ʼ���ٽ���
	InitializeCriticalSection(&m_cs);
}
/*
 * ��������
 */
CClient::~CClient()
{
	closesocket(m_socket);			//�ر��׽���
	m_socket = INVALID_SOCKET;		//�׽�����Ч
	DeleteCriticalSection(&m_cs);	//�ͷ��ٽ�������	
	CloseHandle(m_hEvent);			//�ͷ��¼�����
}

/*
 * �������ͺͽ��������߳�
 */
BOOL CClient::StartRuning(void)
{
	m_bConning = TRUE;//��������״̬

	//�������������߳�
	unsigned long ulThreadId;
	m_hThreadRecv = CreateThread(NULL, 0, RecvDataThread, this, 0, &ulThreadId);
	if(NULL == m_hThreadRecv)
	{
		return FALSE;
	}else{
		CloseHandle(m_hThreadRecv);
	}

	//�������տͻ������ݵ��߳�
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
 * ���տͻ�������
 */
DWORD  CClient::RecvDataThread(void* pParam)
{
	CClient *pClient = (CClient*)pParam;	//�ͻ��˶���ָ��
	int		reVal;							//����ֵ
	char	temp[MAX_NUM_BUF];				//��ʱ����

	memset(temp, 0, MAX_NUM_BUF);
	
	for (;pClient->m_bConning;)				//����״̬
	{
		reVal = recv(pClient->m_socket, temp, MAX_NUM_BUF, 0);	//��������
		
		//������󷵻�ֵ
		if (SOCKET_ERROR == reVal)
		{
			int nErrCode = WSAGetLastError();
			
			if ( WSAEWOULDBLOCK == nErrCode )	//�������ݻ�����������
			{
				continue;						//����ѭ��
			}else if (WSAENETDOWN == nErrCode ||//�ͻ��˹ر�������
					 WSAETIMEDOUT == nErrCode ||
					WSAECONNRESET == nErrCode )			
			{
				break;							//�߳��˳�				
			}
		}
		
		//�ͻ��˹ر�������
		if ( reVal == 0)	
		{
			break;
		}
	
		//�յ�����
		if (reVal > HEADERLEN)			
		{		
			pClient->HandleData(temp);		//��������

			SetEvent(pClient->m_hEvent);	//֪ͨ���������߳�

			memset(temp, 0, MAX_NUM_BUF);	//�����ʱ����
		}
		
		Sleep(TIMEFOR_THREAD_CLIENT);		//�߳�˯��
	}
	
	pClient->m_bConning = FALSE;			//��ͻ��˵����ӶϿ�
	SetEvent(pClient->m_hEvent);			//֪ͨ���������߳��˳�
	
	return 0;								//�߳��˳�
}

/*
 * //��ͻ��˷�������
 */
DWORD CClient::SendDataThread(void* pParam)	
{
	CClient *pClient = (CClient*)pParam;//ת����������ΪCClientָ��

	for (;pClient->m_bConning;)//����״̬
	{	
		//�յ��¼�֪ͨ
		if (WAIT_OBJECT_0 == WaitForSingleObject(pClient->m_hEvent, INFINITE))
		{
			//���ͻ��˵����ӶϿ�ʱ�����������߳����˳���Ȼ����̺߳��˳����������˳���־
			if (!pClient->m_bConning)
			{
				pClient->m_bExit = TRUE;
				break ;
			}

			//�����ٽ���
			EnterCriticalSection(&pClient->m_cs);		
			//��������
			phdr pHeader = (phdr)pClient->m_data.buf;
			int nSendlen = pHeader->len;

			int val = send(pClient->m_socket, pClient->m_data.buf, nSendlen,0);
			//�����ش���
			if (SOCKET_ERROR == val)
			{
				int nErrCode = WSAGetLastError();
				if (nErrCode == WSAEWOULDBLOCK)//�������ݻ�����������
				{
					continue;
				}else if ( WSAENETDOWN == nErrCode || 
						  WSAETIMEDOUT == nErrCode ||
						  WSAECONNRESET == nErrCode)//�ͻ��˹ر�������
				{
					//�뿪�ٽ���
					LeaveCriticalSection(&pClient->m_cs);
					pClient->m_bConning = FALSE;	//���ӶϿ�
					pClient->m_bExit = TRUE;		//�߳��˳�
					break;			
				}else {
					//�뿪�ٽ���
					LeaveCriticalSection(&pClient->m_cs);
					pClient->m_bConning = FALSE;	//���ӶϿ�
					pClient->m_bExit = TRUE;		//�߳��˳�
					break;
				}				
			}
			//�ɹ���������
			//�뿪�ٽ���
			LeaveCriticalSection(&pClient->m_cs);
			//�����¼�Ϊ���ź�״̬
			ResetEvent(&pClient->m_hEvent);	
		}	
		
	}

	return 0;
}
/*
 *  ������ʽ,�������
 */
void CClient::HandleData(const char* pExpr)	
{
	memset(m_data.buf, 0, MAX_NUM_BUF);//���m_data
	
    //����ǡ�byebye�����ߡ�Byebye��
	if (BYEBYE == ((phdr)pExpr)->type)		
	{
		EnterCriticalSection(&m_cs);
		phdr pHeaderSend = (phdr)m_data.buf;				//���͵�����		
		pHeaderSend->type = BYEBYE;							//��������
		pHeaderSend->len = HEADERLEN + strlen("OK");		//���ݰ�����
		memcpy(m_data.buf + HEADERLEN, "OK", strlen("OK"));	//�������ݵ�m_data"
		LeaveCriticalSection(&m_cs);
		
	}else{//�������ʽ
		
		int nFirNum;		//��һ������
		int nSecNum;		//�ڶ�������
		char cOper;			//���������
		int nResult;		//������
		//��ʽ����������
		sscanf(pExpr + HEADERLEN, "%d%c%d", &nFirNum, &cOper, &nSecNum);
		
		//����
		switch(cOper)
		{
		case '+'://��
			{
				nResult = nFirNum + nSecNum;
				break;
			}
		case '-'://��
			{
				nResult = nFirNum - nSecNum;
				break;
			}
		case '*'://��
			{
				nResult = nFirNum * nSecNum;				
				break;
			}
		case '/'://��
			{
				if (ZERO == nSecNum)//��Ч������
				{
					nResult = INVALID_NUM;
				}else
				{
					nResult = nFirNum / nSecNum;	
				}				
				break;
			}
		default:
			nResult = INVALID_OPERATOR;//��Ч������
			break;			
		}
		
		//���������ʽ�ͼ���Ľ��д���ַ�������
		char temp[MAX_NUM_BUF];
		char cEqu = '=';
		sprintf(temp, "%d%c%d%c%d",nFirNum, cOper, nSecNum,cEqu, nResult);

		//�������
		EnterCriticalSection(&m_cs);
		phdr pHeaderSend = (phdr)m_data.buf;				//���͵�����		
		pHeaderSend->type = EXPRESSION;						//��������Ϊ�������ʽ
		pHeaderSend->len = HEADERLEN + strlen(temp);		//���ݰ��ĳ���
		memcpy(m_data.buf + HEADERLEN, temp, strlen(temp));	//�������ݵ�m_data
		LeaveCriticalSection(&m_cs);
		
	}
}

