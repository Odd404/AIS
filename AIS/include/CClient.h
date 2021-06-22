#ifndef CCLIENT_H_
#define CCLIENT_H_

#include "CConnPool.h"
#include <mutex>
#include <condition_variable>

//���粿�֣���ͬ����������

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef size_t SOCKET
#endif

#define MAX_NUM_BUF 4096
using namespace std;

class CClient
{
private:
	SOCKET                  m_socket;			//�׽���
	sockaddr_in             m_addr;			//��ַ
	bool m_bConning;			//�ͻ�������״̬
	bool m_bExit;				//�߳��˳�
private:
	CClient();

public:
	CClient(const SOCKET sClient, const sockaddr_in& addrClient);

	static void RecvDataThread(void* pParam);	//���տͻ�������
	static void HandleDataThread(void* pParam);	//����aisMessage

	virtual ~CClient();

public:
	void GetConnPoolInit();					//��ʼ�����ݿ����ӳ�

	bool StartRuning();					//�������ͺͽ��������߳�
	bool ParseData(const char* pExpr);		//������ʽ����������

	bool IsConning(void)
	{
		return m_bConning;
	}

	void DisConning(void)					//�Ͽ���ͻ��˵�����
	{
		m_bConning = false;
	}

	bool IsExit(void)						//���պͷ����߳��Ƿ��Ѿ��˳�
	{
		return m_bExit;
	}

	char* Getm_addr()
	{
		return inet_ntoa(m_addr.sin_addr);;
	}
};

#endif