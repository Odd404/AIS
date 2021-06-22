/**
 * @file path_utils.h
 *
 * ʹ��C++ 17
*/
#ifndef SECPLATFORM_COMMON_PATHUTILS_H
#define SECPLATFORM_COMMON_PATHUTILS_H

#include <string>
#include <algorithm>
#include <direct.h>
namespace path_utils
{

    /**
    * #purpose	        : ��õ�ǰ����Ŀ¼
    * #note	            :
    * #return	        : ��ǰ����Ŀ¼
    */

    static const std::string GetCurrentDirPath()
	{
		char dirPath[256] = {0};
		getcwd(dirPath, sizeof(dirPath));
		return dirPath;
	}

    /**
    * #purpose	        : ��õ�ǰ����Ŀ¼�µ�ĳ���ļ�·��
    * #note	            : �������·���Ƿ����
    * #param path		: ��·��
    * #return	        : ����·��
    */

	static const std::string GetPath(std::string path){
	    string filePath = GetCurrentDirPath();

#ifdef WIN32
        string temp = path;
        std::replace(temp.begin(),temp.end(),'/','\\');
        filePath += temp;
        return filePath;
#else
        filePath += path;
        return filePath;
#endif
    }

}

#endif // SECPLATFORM_COMMON_PATHUTILS_H