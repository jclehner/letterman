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
using namespace std;

namespace letterman {

	namespace {

		void capitalize(string& str) {
			transform(str.begin(), str.end(), str.begin(), ::toupper);
		}

		string readlink(const string& path)
		{
			char buf[1024];
			ssize_t len = ::readlink(path.c_str(), buf, sizeof(buf) - 1);
			if (len == -1) {
				throw ErrnoException("readlink");
			}

			return string(buf, len);
		}

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
			capitalize(guid);

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

		string findDiskWithMbrId(uint32_t id)
		{
			for (auto& disk : DevTree::getDisks(Properties())) {
#ifdef __linux__
				if (!disk.second["ID_DRIVE_FLOPPY"].empty()) continue;
#endif

				ifstream mbr(disk.first.c_str());
				if (!mbr.good() || !mbr.seekg(440)) {
					continue;
				}

				uint32_t mbrId;
				if(!mbr.read(reinterpret_cast<char*>(&mbrId), 4)) {
					continue;
				}

				if(le32toh(mbrId) == id) {
					return disk.first;
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
		capitalize(_guid);
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
		ostr << hex << setfill('0');

		for (size_t i = 0; i < _data.size(); i += 16) {
			if (i != 0) ostr << endl;
			ostr << string(padding, ' ') << setw(4) << i << " ";
			string ascii;
			for (size_t k = 0; k != 16; ++k) {
				if (k == 8) ostr << " ";
				if (i + k < _data.size()) {
					int c = _data[i + k] & 0xff;
					ostr << " " << setw(2) << c;
					ascii += isprint(c) ? c : '.';
				} else {
					ostr << "   ";
				}
			}

			ostr << "  |" << ascii << "|";
		}

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
			disk = findDiskWithMbrId(_disk);
		} else {
			// TODO handle the not-so-fringe case where getDisks() returns
			// more than one result
			disk = result.begin()->first;
		}

		if (disk.empty()) {
			return kOsNameUnknown;
		} 
		
		// We have a disk name, but no partition yet. Do the lookup 
		// again, this time explicitly specifying the disk and
		// ignoring the kPropMbrId

		criteria[DevTree::kPropMbrId] = DevTree::kIgnoreValue;
		criteria[DevTree::kPropDevice] = disk;
		result = DevTree::getDisks(criteria);

		if (result.empty()) {
			// This shouldn't happen, since findDiskWithMbrId returned
			// a disk...
			return kOsNameUnknown;
		}

		// Remove the disk, as we are searching for the partition now
		criteria[DevTree::kPropDevice] = DevTree::kIgnoreValue;

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
}
