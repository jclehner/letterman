#ifndef LETTERMAN_MBR_H
#define LETTERMAN_MBR_H
#include <iostream>
#include <stdint.h>

namespace letterman {
	struct MBR
	{
		struct Partition
		{
			uint8_t bootable;
			char chsFirst[3];
			uint8_t type;
			char chsLast[3];
			uint32_t lbaStart;
			uint32_t lbaSize;
		} __attribute__((packed));

		char code[440];
		uint32_t id;
		char zero[2];
		Partition partitions[4];
		uint16_t sig;

		bool read(std::istream& in);

		static bool isExtended(const Partition& partition) {
			switch (partition.type) {
				case 0x05: // CHS extended, but (ab)used as LBA
				case 0x0f: // LBA extended
				case 0x85: // Linux extended
					return true;

				default:
					return false;
			}
		}


	} __attribute__((packed));

	static_assert(sizeof(MBR) == 512, "MBR is not 512 bytes");
}
#endif
