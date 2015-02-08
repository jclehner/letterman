#include "endian.h"
#include "mbr.h"

namespace letterman {
	bool MBR::read(std::istream& in) 
	{
		if (!in.read(reinterpret_cast<char*>(this), 512)) {
			return false;
		}

		if((sig = le16toh(sig)) != 0xaa55) {
			return false;
		}

		id = le32toh(id);

		for (unsigned i = 0; i != 4; ++i) {
			partitions[i].lbaStart = le32toh(partitions[i].lbaStart);
			partitions[i].lbaSize = le32toh(partitions[i].lbaSize);
		}

		return true;
	}
}
