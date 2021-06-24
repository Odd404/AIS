/*
* CConnPool.cpp
*
*  Created on: Mar 15, 2018
*      Author: 
* 
*  修改于：2021.6.9
*		使用c++ 标准库
*  
*  说明：
*		使用config_file读取配置文件
*		初始化时产生 maxsize/2 个数据库连接，然后根据需求再增加，最大maxsize
*/
#include "general.h"

#include <stdexcept>
#include <exception>

#include "CConnPool.h"
#include "path_utils.h"
#include "config_file.h"

CConnPool *CConnPool::connPool = NULL;


void CConnPool::InitConnpool(string url, string user, string password, int maxSize)
{
	this->maxSize = maxSize;
	this->curSize = 0;
	this->user = user;
	this->password = password;
	this->url = url;

	try
	{
		this->driver = sql::mysql::get_driver_instance();
	}
	catch (sql::SQLException &e)
	{
		perror("驱动连接出错.\n");
	}
	catch (std::runtime_error &e)
	{
		perror("运行出错了.\n");
	}

	this->InitConnection(maxSize / 2);

}

CConnPool::CConnPool(string url, string user, string password, int maxSize)
{
	//读取配置文件
	try
	{
		string filePath = path_utils::GetPath("/config/AisDaemon.conf");
		ConfigFile configFile(filePath, "=", "#", "EndConfigFile");

		url = configFile.read("url", url);
		user = configFile.read("username", user);
		password = configFile.read("password", password);
		maxSize = configFile.read("maxpool", maxSize);
	}
	catch (...)
	{
		perror("读取配置文件出错.\n");
	}
	
	this->maxSize = maxSize;
	this->curSize = 0;
	this->user = user;
	this->password = password;
	this->url = url;

	try
	{
		this->driver = sql::mysql::get_driver_instance();
	}
	catch (sql::SQLException &e)
	{
		perror("驱动连接出错.\n");
	}
	catch (std::runtime_error &e)
	{
		perror("运行出错了.\n");
	}

	this->InitConnection(maxSize / 2);

}

CConnPool *CConnPool::GetInstance()
{
	if (connPool == NULL || connPool->connList.size() == 0)
	{
		connPool = new CConnPool("tcp://127.0.0.1:3306", "root", "123456", 10);
	}

	return connPool;
}

void CConnPool::InitConnection(int num)
{
	Connection *conn = NULL;

	std::lock_guard<std::mutex> guard(lock);

	for (int i = 0; i < num; ++i)
	{
		conn = CreateConnection();

		if (conn)
		{
			connList.push_back(conn);

			++curSize;
		}
		else
		{
			perror("创建Connection出错.");
		}
	}

}

Connection *CConnPool::CreateConnection()
{
	Connection *_conn = NULL;

	try
	{
		if (driver == NULL)
		{
			perror("driver is null");
		}

		//.c_str()后传入的参数类型是 char 字符串 ，不改是string对象
		//Mysql的参数是自定义参数，与string类型不一致
		//_conn = driver->connect(url, user, password); //建立连接
		_conn = driver->connect(url.c_str(), user.c_str(), password.c_str()); //建立连接

		if (_conn == NULL)
		{
			perror("_conn is null");
		}

		perror("连接数据库成功.");

		return _conn;
	}
	catch (sql::SQLException &e)
	{
		perror(e.what());
	}
	catch (std::runtime_error &e)
	{
		perror(e.what());
	}
	catch (std::bad_alloc &excp) 
	{
		perror(excp.what());
	}

	return NULL;
}

Connection *CConnPool::GetConnection()
{
	Connection *conn;

	std::lock_guard <std::mutex> guard(lock);

	if (connList.size() > 0)
	{
		conn = connList.front();
		connList.pop_front();

		if (conn->isClosed())
		{
			delete conn;

			conn = CreateConnection();
		}

		if (conn == NULL)
		{
			--curSize;
		}

		
		return conn;
	}
	else
	{
		if (curSize < maxSize)
		{
			conn = CreateConnection();

			if (conn)
			{
				++curSize;

				return conn;
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			cout << "超出最大连接." << endl;
			return NULL;
		}
	}
}

void CConnPool::ReleaseConnection(Connection *conn)
{
	if (conn)
	{
		std::lock_guard<std::mutex> guard(lock);

		connList.push_back(conn);
	}
}

CConnPool::~CConnPool()
{
	this->DestoryConnPool();
}

void CConnPool::DestoryConnPool()
{
	list<Connection *>::iterator iter;

	std::lock_guard<std::mutex> guard(lock);

	for (iter = connList.begin(); iter != connList.end(); ++iter)
		this->DestoryConnection(*iter);

	curSize = 0;
	connList.clear();

}

void CConnPool::DestoryConnection(Connection *conn)
{
	if (conn)
	{
		try
		{
			conn->close();
		}
		catch (sql::SQLException &e)
		{
			perror(e.what());
		}
		catch (std::exception &e)
		{
			perror(e.what());
		}

		delete conn;
	}
}
