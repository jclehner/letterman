#include <netinet/in.h>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <cctype>
#include "exception.h"
#include "mounted_devices.h"
using namespace std;

namespace letterman {
	namespace {

		string toString(char* buf, size_t len = 0)
		{
			unique_ptr<char> p(buf);
			return len ? string(buf, len) : string(buf);
		}

		// poor man's conversion from windows wide strings
		// with 16-byte chars. we simply ignore all chars
		// where the second byte is non-zero
		string fromWstring(string wstr)
		{
			if (wstr.size() % 2) {
				throw invalid_argument("wstr");
			}

			string ret;

			for(string::size_type i = 0; i < wstr.size(); i += 2) {
				ret += wstr[i + 1] ? '?' : wstr[i];
			}

			return ret;
		}

		string parseGuid(const char* buf, size_t offset)
		{
			ostringstream ostr;

			const char* p = buf + offset;
			uint32_t a = *reinterpret_cast<const uint32_t*>(p + 0);
			uint16_t b = *reinterpret_cast<const uint16_t*>(p + 4);
			uint16_t c = *reinterpret_cast<const uint16_t*>(p + 6);
			uint16_t d = htons(*reinterpret_cast<const uint16_t*>(p + 8));

			ostr << uppercase << hex << setfill('0');
			ostr << setw(8) << a << "-";
			ostr << setw(4) << b << "-";
			ostr << setw(4) << c << "-";
			ostr << setw(4) << d << "-";

			for (int i = 10; i != 16; ++i) {
				int c = *(p + i) & 0xff;
				ostr << setw(2) << c;
			}

			return ostr.str();
		}

		Device* createDevice(const string& data)
		{
			const char* buf = data.c_str();
			size_t len = data.size();

			if (len == 12) {
				uint32_t disk = *reinterpret_cast<const uint32_t*>(buf);
				uint64_t offset = *reinterpret_cast<const uint64_t*>(buf + 4);

				return new MbrPartitionDevice(data, disk, offset);
			} else if (len >= 8) {
				uint64_t magic = *reinterpret_cast<const uint64_t*>(buf);
				if (len == 24 && magic == 0x3a44493a4f494d44LLU) { // "DMIO:ID:"
				{
					return new GuidPartitionDevice(data, parseGuid(buf, 8));
				}
				} else if (magic == 0x005c003f003f005cLLU // "\??\"
						|| magic == 0x005f003f003f005fLLU) { // "_??_" 
					if(len >= (36 + 2) * 2) {
						string bytes(fromWstring(data));

						// Data is composed of the "Device Instance Path", with an
						// appended GUID specifying the "Device Interface"
						// (http://msdn.microsoft.com/en-us/library/windows/hardware/ff545813%28v=vs.85%29.aspx)
						// Note that the GUID is surrounded by {}, so to parse it
						// we must add 1 for uuid_parse()
						size_t guidBegin = bytes.size() - (36 + 2);

						string instancePath(bytes.substr(4, guidBegin - 4));
						string intfGuid(bytes.substr(guidBegin + 1, 36));

						string::size_type pos;

						while ((pos = instancePath.find('#')) != string::npos) {
							instancePath[pos] = '\\';
						}

						if(instancePath[instancePath.size() - 1] == '\\') {
							instancePath.resize(instancePath.size() - 1);
						}

						return new GenericDevice(data, instancePath, intfGuid);
					}
				}
			}
			return new RawDevice(data);
		}
	}

	MountedDevices::MountedDevices(const char* filename)
	{
		_hive = hivex_open(filename, 0);
		if (!_hive) {
			throw ErrnoException("hivex_open");
		}

		_node = hivex_root(_hive);
		if (!_node) {
			throw ErrnoException("hivex_root");
		}

		_node = hivex_node_get_child(_hive, _node, "MountedDevices");
		if (!_node) {
			throw ErrnoException("hivex_node_get_child");
		}
	}

	MountedDevices::~MountedDevices()
	{
		hivex_close(_hive);
	}

	vector<Device*> MountedDevices::list(int flags)
	{
		hive_value_h *values = hivex_node_values(_hive, _node);
		if (!values) {
			throw ErrnoException("hivex_node_values");
		}

		vector<Device*> devices;

		for (; *values; ++values) {
			int letter = 0;
			string key(toString(hivex_value_key(_hive, *values)));

			if (key.find("\\DosDevices\\") == string::npos) {
				// do something here
			} else if(key.size() != 14 || key[key.size() - 1] != ':') {
				throw runtime_error("Invalid key " + key);
			} else {
				letter = toupper(key[key.size() - 2]);
			}

			hive_type type;
			size_t len;
			char *buf = hivex_value_value(_hive, *values, &type, &len);
			if (!buf) {
				throw ErrnoException("hivex_value_value");
			}

			if (!letter && !(flags & LIST_WITHOUT_LETTER)) {
				continue;
			}

			Device* device(createDevice(toString(buf, len)));
			device->setLetter(letter);
			devices.push_back(device);
		}

		return devices;
	}
}



