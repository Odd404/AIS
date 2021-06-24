#ifndef CCLIENT_H_
#define CCLIENT_H_

#include "CConnPool.h"
#include <mutex>
#include <condition_variable>

//网络部分，不同环境的设置

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef size_t SOCKET;
#endif

#define MAX_NUM_BUF 4096
using namespace std;

class CClient
{
private:
	SOCKET                  m_socket;			//套接字
	sockaddr_in             m_addr;			//地址
	bool m_bConning;			//客户端连接状态
	bool m_bExit;				//线程退出
private:
	CClient();

public:
	CClient(const SOCKET sClient, const sockaddr_in& addrClient);

	static void RecvDataThread(void* pParam);	//接收客户端数据
	static void HandleDataThread(void* pParam);	//处理aisMessage

	virtual ~CClient();

public:
	void GetConnPoolInit();					//初始化数据库连接池

	bool StartRuning();					//创建发送和接收数据线程
	bool ParseData(const char* pExpr);		//计算表达式－处理数据

	bool IsConning(void)
	{
		return m_bConning;
	}

	void DisConning(void)					//断开与客户端的连接
	{
		m_bConning = false;
	}

	bool IsExit(void)						//接收和发送线程是否已经退出
	{
		return m_bExit;
	}

	char* Getm_addr()
	{
		return inet_ntoa(m_addr.sin_addr);;
	}
};

#endif