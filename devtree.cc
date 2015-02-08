#include <iostream>
#include <fstream>
#include <sstream>
#include "devtree.h"
#include "util.h"
#include "mbr.h"
using namespace std;

namespace letterman {
	namespace {
		void fillMbrIdProp(Properties& props)
		{
			if (!props[DevTree::kPropMbrId].empty()) return;

			if (DevTree::isDisk(props)) {
				ifstream in(props[DevTree::kPropDeviceReadable]);
				MBR mbr;

				if (mbr.read(in)) {
					ostringstream ostr;
					ostr << setw(8) << setfill('0') << hex << mbr.id;

					props[DevTree::kPropMbrId] = ostr.str();
				} else {
					// TODO warn?
				}
			}
		}
	}

	// The first char is NUL, so we don't have collisions with
	// any property values (which are all human readable).
	const string DevTree::kIgnoreValue("\01", 2);
	const string DevTree::kAnyValue("\02", 2);
	const string DevTree::kNoValue("\03", 2);

	const string DevTree::kNoMatchIfSetAsPropKey("\04", 2);

	const string DevTree::kPropDiskId = "kPropDiskId";
	const string DevTree::kPropIsNtfs = "kPropIsNtfs";

	size_t DevTree::blockSize(const Properties& props)
	{
		auto iter = props.find(kPropLbaSize);
		if (iter != props.end() && !iter->second.empty()) {
			return util::fromString<size_t>(iter->second);
		} else {
			return 512;
		}
	}

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

			fillMbrIdProp(e.second);

			if (props.empty() || DevTree::arePropsMatching(e.second, props)) {
				ret.insert(e);
			}
		}

		return ret;
	}

}

// OS-specific stuff is in devtree_<os>.cc
