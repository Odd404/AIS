/*
* CConnPool.h
*
*  Created on: Mar 15, 2018
*      Author: 
*/

#ifndef SRC_CCONNPOOL_H_
#define SRC_CCONNPOOL_H_

#include <list>
#include <string>
#include <mutex>

#include <mysql_connection.h>
#include <mysql_driver.h>
#include <cppconn/exception.h>
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/resultset.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>

using namespace sql;
using namespace std;

class CConnPool 
{
private:
	int curSize; //��ǰ�ѽ��������ݿ���������
	int maxSize; //���ӳ��ж����������ݿ�������

	string user = "root";
	string password = "123456";
	string url = "tcp://127.0.0.1:3306";
	int maxPool = 10;

	list<Connection *> connList;									//���ӳص���������  STL list ˫������
	std::mutex lock;

	static CConnPool *connPool;
	Driver *driver;

private:
	CConnPool();
	Connection* CreateConnection();				//����һ������

	void InitConnection(int iInitialSize);		//��ʼ�����ݿ����ӳ�
	void DestoryConnection(Connection *conn);	//�������ݿ����Ӷ���
	void DestoryConnPool();						//�������ݿ����ӳ�

	CConnPool(string url, string user, string password, int maxSize); //���췽��

public:
	~CConnPool();

	void InitConnpool(string url, string user, string password, int maxSize);
	Connection* GetConnection();

	void ReleaseConnection(Connection* conn);

	static CConnPool* GetInstance();
};

#endif /* SRC_CCONNPOOL_H_ */