#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "device.h"
using namespace std;

namespace letterman {

	namespace {

		void capitalize(string& str) {
			transform(str.begin(), str.end(), str.begin(), ::toupper);
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

	}

	DeviceSelector::DeviceSelector(char letter, const string& guid)
	: _letter(letter), _guid(guid)
	{
		capitalize(_guid);
	}

	bool DeviceSelector::matches(const string& key) const
	{
		if (_letter) {
			return key == string("\\DosDevices\\") + _letter + ":";
		} else if (!_guid.empty()) {
			return key == "\\??\\Volume{" + _guid + "}";
		} else {
			return false;
		}
	}

	string RawDevice::toString(int padding) const
	{
		ostringstream ostr;

		for (size_t i = 0; i < _data.size(); i += 16) {
			if (i != 0) ostr << endl;
			ostr << string(padding, ' ') << setw(4) << setfill('0') << setbase(16) << i << " ";
			string ascii;
			for (size_t k = 0; k != 16; ++k) {
				if (k == 8) ostr << " ";
				if (i + k < _data.size()) {
					int c = _data[i + k] & 0xff;
					ostr << " " << setw(2) << setfill('0') << setbase(16) << c;
					ascii += isprint(c) ? c : '.';
				} else {
					ostr << "   ";
				}
			}

			ostr << "  |" << ascii << "|";
		}

		return ostr.str();
	}

	string MbrPartitionDevice::toString(int padding) const
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

	string GuidPartitionDevice::toString(int padding) const
	{
		ostringstream ostr(string(padding, ' '));
		ostr << "GUID Partition " << _guid;
		return ostr.str();
	}

	string GenericDevice::toString(int padding) const
	{
		ostringstream ostr(string(padding, ' '));
		ostr << devInterfaceGuidToName(_guid) << " " << _path;
		return ostr.str();
	}
}
