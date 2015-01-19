#include "devtree.h"
using namespace std;

namespace letterman {

	// The first char is NUL, so we don't have collisions with
	// any property values (which are all human readable).
	const std::string DevTree::kIgnoreValue("\0\1", 2);
	const std::string DevTree::kAnyValue("\0\2", 2);
	const std::string DevTree::kNoValue("\0\3", 2);

	bool DevTree::arePropsMatching(
			const Properties& all, const Properties& criteria)
	{
		for (auto& e : criteria) {
			if (e.first == kIgnoreValue) continue;

			auto iter = all.find(e.first);
			bool exists = iter != all.end();

			if (e.first == kAnyValue && !exists) {
				return false;
			}

			if (e.first == kNoValue && exists) {
				return false;
			}

			if (e.second != iter->second) {
				return false;
			}
		}

		return true;
	}
}

// OS-specific stuff is in devtree_<os>.cc
