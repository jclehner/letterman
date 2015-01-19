#ifndef LETTERMAN_DEVTREE_H
#define LETTERMAN_DEVTREE_H
#include <string>
#include <map>
#include <set>

namespace letterman {

	typedef std::map<std::string, std::string> Properties;

	class DevTree
	{
		public:

		static const std::string kPropDevice;
		static const std::string kPropFsLabel;
		static const std::string kPropMajor;
		static const std::string kPropMinor;
		static const std::string kPropPartUuid;

		static const std::string kIgnoreValue;
		static const std::string kAnyValue;
		static const std::string kNoValue;

		static std::map<std::string, Properties> getDisks(
				const Properties& criteria);
		static std::map<std::string, Properties> getPartitions(
				const Properties& criteria);

		static bool arePropsMatching(
				const Properties& all, const Properties& criteria);
	};
}
#endif
