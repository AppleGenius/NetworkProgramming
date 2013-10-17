#define	BUF_SZIE	64
#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")


int main(int argc, char* argv[])
{

	WSADATA			wsd;			//WSADATA����
	SOCKET			s;				//�׽���
	SOCKADDR_IN		servAddr;		//��������ַ
	char			buf[BUF_SZIE];	//�������ݻ�����

	//��ʼ���׽��ֶ�̬��
	if (WSAStartup(MAKEWORD(2,2), &wsd) != 0)
	{
		printf("WSAStartup failed!\n");
		return 1;
	}

	//�����׽���
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == INVALID_SOCKET)
	{
		printf("socket() failed; %d\n", WSAGetLastError());
		WSACleanup();//�ͷ��׽�����Դ
		return 1;
	}

	int nErrCode;	//����ֵ
	int nBufLen;	//�������ݻ�������С
	int nOptlen = sizeof(nBufLen);
	//��ȡ�������ݻ�������С
	nErrCode = getsockopt(s, SOL_SOCKET, SO_RCVBUF,(char*)&nBufLen, &nOptlen);
	if (SOCKET_ERROR == nErrCode)
	{
		//����ʧ��
	}

	//���ý������ݻ�����Ϊԭ����10��
	nBufLen *= 10;
	nErrCode = setsockopt(s, SOL_SOCKET, SO_RCVBUF,(char*)&nBufLen, nOptlen);
	if (SOCKET_ERROR == nErrCode)
	{
		//ʧ�ܴ���
	}

	//�������ϵͳ�������ݻ������Ƿ�ɹ�
	int uiNewRcvBuf;
	getsockopt(s, SOL_SOCKET, SO_RCVBUF,(char*)&uiNewRcvBuf, &nOptlen);
	if (SOCKET_ERROR == nErrCode || uiNewRcvBuf != nBufLen)
	{
		//ʧ�ܴ���
	}


	//��������ַ
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons((short)5000);			//�˿�
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);	//IP

	//��
	if (bind(s, (SOCKADDR *)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
	{
		printf("bind() failed: %d\n", WSAGetLastError());
		closesocket(s);	//�ر��׽���
		WSACleanup();	//�ͷ��׽�����Դ
		return 1;
	}	

	//��������
	SOCKADDR_IN clientAddr;
	int nClientLen = sizeof(clientAddr);
	ZeroMemory(buf, BUF_SZIE);
	if(recvfrom(s, buf, BUF_SZIE, 0, (SOCKADDR*)&clientAddr, &nClientLen) == SOCKET_ERROR)
	{
		printf("recvfrom() failed: %d\n", WSAGetLastError());
		closesocket(s);	//�ر��׽���
		WSACleanup();	//�ͷ��׽�����Դ
		return 1;
	}	

	printf("%s\n", buf);//���"MyUDP"

	closesocket(s);		//�ر��׽���
	WSACleanup();		//�ͷ��׽�����Դ
	return 0;
}

