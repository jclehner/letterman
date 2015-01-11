#include <iostream>
#include <sstream>
#include <iomanip>
#include "device.h"
using namespace std;

namespace letterman {
	string RawDevice::toString(int padding)
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

	string MbrPartitionDevice::toString(int padding)
	{
		ostringstream ostr(string(padding, ' '));
		ostr << "MBR Disk 0x";
		ostr << setw(8) << setfill('0') << setbase(16) << _disk;
		ostr << " @ 0x";
		ostr << setw(16) << setfill('0') << setbase(16) << _offset;

		return ostr.str();
	}

	string GuidPartitionDevice::toString(int padding)
	{
		ostringstream ostr(string(padding, ' '));
		ostr << "GUID Partition " << _guid;
		return ostr.str();
	}

	string GenericDevice::toString(int padding)
	{
		ostringstream ostr(string(padding, ' '));
		ostr << "Device " << _path ;
		return ostr.str();
	}
}
