/**
 * @file path_utils.h
 *
 * 使用C++ 17
*/
#ifndef SECPLATFORM_COMMON_PATHUTILS_H
#define SECPLATFORM_COMMON_PATHUTILS_H

#include <string>
#include <algorithm>
#include <direct.h>
namespace path_utils
{

    /**
    * #purpose	        : 获得当前程序目录
    * #note	            :
    * #return	        : 当前程序目录
    */

    static const std::string GetCurrentDirPath()
	{
		char dirPath[256] = {0};
		getcwd(dirPath, sizeof(dirPath));
		return dirPath;
	}

    /**
    * #purpose	        : 获得当前程序目录下的某个文件路径
    * #note	            : 不会检查该路径是否存在
    * #param path		: 子路径
    * #return	        : 完整路径
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