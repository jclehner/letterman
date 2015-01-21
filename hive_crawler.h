#ifndef LETTERMAN_HIVE_CRAWLER_H
#define LETTERMAN_HIVE_CRAWLER_H
#include <string>
#include <set>

namespace letterman {

	std::set<std::string> getAllSysDrives();

	std::string hiveFromSysDrive(const std::string& path);

	std::string hiveFromSysRoot(const std::string& path);

	std::string hiveFromSysDir(const std::string& path);

	std::string hiveFromCfgDir(const std::string& path);
}

#endif
