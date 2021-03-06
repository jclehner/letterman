#ifndef LETTERMAN_HIVE_CRAWLER_H
#define LETTERMAN_HIVE_CRAWLER_H
#include <iostream>
#include <string>
#include <set>

namespace letterman {

	std::string hiveFromSysDrive(const std::string& path);

	std::string hiveFromSysRoot(const std::string& path);

	std::string hiveFromSysDir(const std::string& path);

	//std::string hiveFromCfgDir(const std::string& path);

	struct WindowsInstall
	{
		std::string path;
		bool isDevice;

		bool operator<(const WindowsInstall& wi) const
		{ return path < wi.path; }

		friend std::ostream& operator<<(std::ostream& os, const WindowsInstall& wi)
		{
			os << "--sysdrive '" << wi.path << "'";
			return os;
		}
	};

	std::set<WindowsInstall> getAllWindowsInstalls();
}

#endif
