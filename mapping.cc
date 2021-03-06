#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include "exception.h"
#include "devtree.h"
#include "mapping.h"
#include "endian.h"
#include "util.h"
#include "mbr.h"
using namespace std;

namespace letterman {

	namespace {
		static const char* GUID_DEVINTERFACE_CDCHANGER =
			"53F56312-B6BF-11D0-94F2-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_CDROM =
			"53F56308-B6BF-11D0-94F2-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_DISK =
			"53F56307-B6BF-11D0-94F2-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_FLOPPY =
			"53F56311-B6BF-11D0-94F2-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_MEDIUMCHANGER =
			"53F56310-B6BF-11D0-94F2-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_PARTITION =
			"53F5630A-B6BF-11D0-94F2-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_STORAGEPORT =
			"2ACCFE60-C130-11D2-B082-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_TAPE =
			"53F5630B-B6BF-11D0-94F2-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_VOLUME =
			"53F5630D-B6BF-11D0-94F2-00A0C91EFB8B";

		static const char* GUID_DEVINTERFACE_WRITEONCEDISK =
			"53F5630C-B6BF-11D0-94F2-00A0C91EFB8B";

		string devInterfaceGuidToName(string guid)
		{
			util::capitalize(guid);

			if (guid == GUID_DEVINTERFACE_CDCHANGER) return "CD Changer";
			if (guid == GUID_DEVINTERFACE_CDROM) return "CD-ROM";
			if (guid == GUID_DEVINTERFACE_DISK) return "Disk";
			if (guid == GUID_DEVINTERFACE_FLOPPY) return "Floppy";
			if (guid == GUID_DEVINTERFACE_MEDIUMCHANGER) return "Medium Changer";
			if (guid == GUID_DEVINTERFACE_PARTITION) return "Partition";
			if (guid == GUID_DEVINTERFACE_STORAGEPORT) return "Storage Port";
			if (guid == GUID_DEVINTERFACE_TAPE) return "Tape";
			if (guid == GUID_DEVINTERFACE_VOLUME) return "Volume";
			if (guid == GUID_DEVINTERFACE_WRITEONCEDISK) return "Write-Once Disk";

			return guid;
		}

		inline string getPartitionName(const string& disk, unsigned partition)
		{
#ifdef LETTERMAN_MACOSX
			return disk + "s" + util::toString(partition);
#else
			return disk + util::toString(partition);
#endif
		}

		unsigned resolveExtendedPartition(ifstream& in, const size_t blockSize,
				const uint64_t extLbaStart, uint64_t ebrLbaStart,
				const uint64_t searchOffset, unsigned& counter)
		{
			MBR ebr;

			if (!in.seekg(ebrLbaStart * blockSize) || !ebr.read(in)) {
				return 0;
			}


			uint64_t logPartLbaStart =
				ebrLbaStart + ebr.partitions[0].lbaStart;

			ebrLbaStart = extLbaStart + ebr.partitions[1].lbaStart;

			if (logPartLbaStart == searchOffset) {
				return counter;
			} else if (ebrLbaStart != extLbaStart) {
				return resolveExtendedPartition(in, blockSize, extLbaStart,
						ebrLbaStart, searchOffset, ++counter);
			}

			return 0;
		}

		unsigned resolveExtendedPartition(ifstream& in, size_t blockSize,
				uint64_t extLbaStart, uint64_t lbaStart, unsigned& counter)
		{
			return resolveExtendedPartition(in, blockSize, extLbaStart,
					extLbaStart, lbaStart, counter);
		}

		string resolveMbrDiskOrPartition(uint32_t id, uint64_t offset = 0,
				bool useLbaStart = false)
		{
			for (auto& disk : DevTree::getDisks()) {
				const string& device = disk.second[DevTree::kPropDeviceReadable];
				ifstream in(device.c_str());
				MBR mbr;
				if (!in.good() || !mbr.read(in)) {
					continue;
				}

				if (mbr.id == id) {
					if (!useLbaStart) return device;

					size_t blockSize = DevTree::blockSize(disk.second);
					uint64_t lbaStart = offset / blockSize;

					if (lbaStart > UINT64_C(0xffffffff)) continue;

					unsigned counter = 5;

					for (unsigned i = 0; i != 4; ++i) {
						if (!mbr.partitions[i].type) continue;

						uint32_t partLbaStart = mbr.partitions[i].lbaStart;

						if (partLbaStart == lbaStart) {
							return getPartitionName(device, i + 1);
						} else if (MBR::isExtended(mbr.partitions[i])) {
							unsigned partition = resolveExtendedPartition(in,
									blockSize, partLbaStart, lbaStart * blockSize,
									counter);

							if (partition) {
								return getPartitionName(device, partition);
							}
						}
					}
				}
			}

			return "";
		}

	}

	const string Mapping::kOsNameUnknown("\0\1", 2);
	const string Mapping::kOsNameNotAttached("\0\2", 2);

	MappingName::MappingName(char letter, const string& guid)
	: _letter(letter), _guid(guid)
	{
		util::capitalize(_guid);
	}

	std::string MappingName::key() const
	{
		if (_letter) {
			return string("\\DosDevices\\") + _letter + ":";
		} else if (!_guid.empty()) {
			return "\\??\\Volume{" + _guid + "}";
		} else {
			return "";
		}
	}

	string RawMapping::toString(int padding) const
	{
		ostringstream ostr;
		util::hexdump(ostr, _data, padding);
		return ostr.str();
	}

	string MbrPartitionMapping::toString(int padding) const
	{
		ostringstream ostr(string(padding, ' '));
		ostr << setfill('0') << hex;

		ostr << "MBR Disk 0x";
		ostr << setw(8) << _disk;
		ostr << " @ 0x";
		ostr << setw(16) << _offset;
		ostr << " (block " << setw(0) << dec << _offset / 512 << ")";

		return ostr.str();
	}

	string MbrPartitionMapping::osDeviceName() const
	{
		ostringstream ostr;
		ostr << setfill('0') << hex << setw(8) << _disk;

		Properties criteria = {{ DevTree::kPropMbrId, ostr.str() }};

		string disk;
		map<string, Properties> result(DevTree::getDisks(criteria));

		if (result.empty()) {
#ifndef LETTERMAN_LINUX
			disk = resolveMbrDiskOrPartition(_disk, _offset, true);
#else
			disk = resolveMbrDiskOrPartition(_disk);
#endif
		} else {
			// TODO handle the not-so-fringe case where getDisks() returns
			// more than one result
			disk = result.begin()->second[DevTree::kPropDeviceReadable];
		}

		if (disk.empty()) {
			return kOsNameUnknown;
		}
#ifndef LETTERMAN_LINUX
		else {
			return disk;
		}
#endif

		// We have a disk name, but no partition yet. Do the lookup
		// again, this time explicitly specifying the disk and
		// ignoring the kPropMbrId

		criteria[DevTree::kPropMbrId] = DevTree::kIgnoreValue;
		criteria[DevTree::kPropDeviceReadable] = disk;
		result = DevTree::getDisks(criteria);

		if (result.empty()) {
			// This shouldn't happen, since findDiskWithMbrId returned
			// a disk...
			return kOsNameUnknown;
		}

		// Remove the disk, as we are searching for the partition now
		criteria[DevTree::kPropDeviceReadable] = DevTree::kIgnoreValue;

		Properties props = result.begin()->second;

		// Searching for a partition by offset only might yield
		// ambigous results, especially if the partition in question
		// is the first partition with an offset of 2 or 63 blocks
		// (many partitioning utilities do this). We thus limit our
		// search to partitions with a matching disk id.
		criteria[DevTree::kPropDiskId] = props[DevTree::kPropDiskId];

		// Try blocks offset first
		criteria[DevTree::kPropPartOffsetBlocks] = util::toString(_offset / 512);

		result = DevTree::getPartitions(criteria);
		if (!result.empty()) {
			return result.begin()->first;
		}

		// Now try byte offset
		criteria[DevTree::kPropPartOffsetBytes] = util::toString(_offset);
		criteria[DevTree::kPropPartOffsetBlocks] = DevTree::kIgnoreValue;

		result = DevTree::getPartitions(criteria);
		if (!result.empty()) {
			return result.begin()->first;
		}

#ifdef __linux__
		return kOsNameNotAttached;
#else
		// On OSX, most of the above queries will fail, so we can't say
		// that the device is not attached.
		return kOsNameUnknown;
#endif
	}

	string GuidPartitionMapping::osDeviceName() const
	{
		string guid(_guid);

		Properties criteria = {{ DevTree::kPropPartUuid, guid }};
		map<string, Properties> result(DevTree::getPartitions(criteria));
		if (!result.empty()) {
			// TODO handle the fringe case where there is more
			// than one result!
			return result.begin()->first;
		}

		return kOsNameNotAttached;
	}

	string GuidPartitionMapping::toString(int padding) const
	{
		ostringstream ostr(string(padding, ' '));
		ostr << "GUID Partition " << _guid;
		return ostr.str();
	}

	string GenericMapping::toString(int padding) const
	{
		ostringstream ostr(string(padding, ' '));
		ostr << devInterfaceGuidToName(_guid) << " " << _path;
		return ostr.str();
	}

	string GenericMapping::osDeviceName() const
	{
		string path(_path);

		string::size_type pos = path.find("SCSI\\CdRom");
		if (pos == 0) {
			pos = path.find("&Prod_");
			if (pos == string::npos) return kOsNameUnknown;

			// SCSI optical drives are stored as
			// SCSI\CdRom&Ven_<vendor>&Prod_<model>\,
			// whereas udev seems to use <vendor><mode>
			// in ID_MODEL.

			// Remove &Prod_
			path.erase(pos, 6);

			pos = path.find("&Ven_");
			if (pos == string::npos || pos + 5 >= path.size()) {
				return kOsNameUnknown;
			}

			pos += 5;

			string::size_type begin = pos;

			pos = path.find_first_of("\\&", begin);
			if (pos == string::npos) return kOsNameUnknown;

			string model(path.substr(begin, pos - begin));
			Properties criteria = {{ DevTree::kPropHardware, model }};

			map<string, Properties> results(DevTree::getDisks(criteria));
			if (results.empty()) {
				return kOsNameNotAttached;
			} else if (results.size() == 1) {
				return results.begin()->first;
			}
		}

		if ((pos = path.find("IDE\\CdRom")) == 0) {
			// IDE optical drives are stored as
			// IDE\CdRom<vendor>_<model>_<other info>

			// Skip the prefix
			pos += 9;

			if (pos >= path.size()) return kOsNameUnknown;

			string::size_type begin = pos;

			if ((pos = path.find('_', pos)) == string::npos) {
				return kOsNameUnknown;
			}

			// Remove the first underscore. Note that this will not
			// work if the vendor string contains an underscore!

			path.erase(pos);

			string ret;

			for (auto& disk : DevTree::getDisks()) {
				if (path.find(disk.second[DevTree::kPropHardware]) == begin) {
					if (!ret.empty()) {
						// We have more than one match!
						return kOsNameUnknown;
					}
					ret = disk.first;
				}
			}

			return ret.empty() ? kOsNameNotAttached : ret;
		}

		return kOsNameUnknown;
	}
}
