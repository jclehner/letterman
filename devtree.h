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
		static const std::string kPropModel;

		static const std::string kPropMountPoint;
		static const std::string kPropMbrId;
		static const std::string kPropPartOffsetBlocks;
		static const std::string kPropPartOffsetBytes;

		static const std::string kPropDiskId;
		static const std::string kPropIsNtfs;

		static const std::string kIgnoreValue;
		static const std::string kAnyValue;
		static const std::string kNoValue;

		static std::map<std::string, Properties> getDisks(
				const Properties& criteria = Properties())
		{ return getDisksOrPartitions(criteria, true); }

		static std::map<std::string, Properties> getPartitions(
				const Properties& criteria = Properties())
		{ return getDisksOrPartitions(criteria, false); }

		private:

		// When using this as as property key, return false in
		// arePropsMatching. This is used for props that are not
		// available on the current OS (such as kPropMbrId on OS X)
		static const std::string kNoMatchIfSetAsPropKey;

		static std::map<std::string, Properties> getAllDevices();
		static std::map<std::string, Properties> getDisksOrPartitions(
				const Properties& criteria, bool getDisks);

		static bool isDiskOrPartition(const Properties& props, bool isDisk);

		static bool isDisk(const Properties& props)
		{ return isDiskOrPartition(props, true); }

		static bool isPartition(const Properties& props)
		{ return isDiskOrPartition(props, false); }


		static bool arePropsMatching(
				const Properties& all, const Properties& criteria);
	};
}
#endif
