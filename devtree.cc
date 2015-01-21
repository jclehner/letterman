#include <iostream>
#include "devtree.h"
using namespace std;

namespace letterman {

	// The first char is NUL, so we don't have collisions with
	// any property values (which are all human readable).
	const string DevTree::kIgnoreValue("\01", 2);
	const string DevTree::kAnyValue("\02", 2);
	const string DevTree::kNoValue("\03", 2);

	const string DevTree::kNoMatchIfSetAsPropKey("\04", 2);

	const string DevTree::kPropDiskId = "kPropDiskId";
	const string DevTree::kPropIsNtfs = "kPropIsNtfs";

	bool DevTree::arePropsMatching(
			const Properties& all, const Properties& criteria)
	{
		for (auto& crit : criteria) {

			if (crit.second == kIgnoreValue) continue;

			auto iter = all.find(crit.first);
			bool exists = iter != all.end();
			if (crit.first == kNoMatchIfSetAsPropKey) {
				return false;
			}

			if (crit.second == kNoValue && exists) {
				return false;
			}

			if (exists && (crit.second != iter->second && crit.second != kAnyValue)) {
				return false;
			}

			if (!exists)
				return false;
		}

		return true;
	}


	map<string, Properties> DevTree::getDisksOrPartitions(
			const Properties& props, bool getDisks)
	{
		map<string, Properties> ret;

		for (auto& e : getAllDevices()) {

			if(!(getDisks ? isDisk(e.second) : isPartition(e.second))) {
				continue;
			}

			if (props.empty() || DevTree::arePropsMatching(e.second, props)) {
				ret.insert(e);
			}
		}

		return ret;
	}

}

// OS-specific stuff is in devtree_<os>.cc
