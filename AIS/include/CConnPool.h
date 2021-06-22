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
	int curSize; //当前已建立的数据库连接数量
	int maxSize; //连接池中定义的最大数据库连接数

	string user = "root";
	string password = "123456";
	string url = "tcp://127.0.0.1:3306";
	int maxPool = 10;

	list<Connection *> connList;									//连接池的容器队列  STL list 双向链表
	std::mutex lock;

	static CConnPool *connPool;
	Driver *driver;

private:
	CConnPool();
	Connection* CreateConnection();				//创建一个连接

	void InitConnection(int iInitialSize);		//初始化数据库连接池
	void DestoryConnection(Connection *conn);	//销毁数据库连接对象
	void DestoryConnPool();						//销毁数据库连接池

	CConnPool(string url, string user, string password, int maxSize); //构造方法

public:
	~CConnPool();

	void InitConnpool(string url, string user, string password, int maxSize);
	Connection* GetConnection();

	void ReleaseConnection(Connection* conn);

	static CConnPool* GetInstance();
};

#endif /* SRC_CCONNPOOL_H_ */