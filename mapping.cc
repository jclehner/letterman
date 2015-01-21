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

		string findDiskWithMbrId(uint32_t id)
		{
			for (auto& disk : DevTree::getDisks(Properties())) {
				ifstream mbr(disk.first.c_str());
				if (!mbr.good() || !mbr.seekg(510)) {
					continue;
				}

				uint16_t mbrSig;
				if (!mbr.read(reinterpret_cast<char*>(&mbrSig), 2)) {
					continue;
				}

				if (le16toh(mbrSig) != 0xaa55 || !mbr.seekg(440)) {
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

	string GenericMapping::osDeviceName() const
	{
		// FIXME this does not work on OSX, as the data
		// obtained from DiskArbitration don't include the
		// optical drive if it contains no medium.

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
			Properties criteria = {{ DevTree::kPropModel, model }};

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
				if (path.find(disk.second[DevTree::kPropModel]) == begin) {
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
