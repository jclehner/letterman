#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <cctype>
#include "mounted_devices.h"
#include "exception.h"
#include "endian.h"
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
			uint16_t d = htobe16(*reinterpret_cast<const uint16_t*>(p + 8));

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
				uint32_t disk = le32toh(*reinterpret_cast<const uint32_t*>(buf));
				uint64_t offset = le64toh(*reinterpret_cast<const uint64_t*>(buf + 4));

				return new MbrPartitionDevice(data, disk, offset);
			} else if (len >= 8) {
				uint64_t magic = *reinterpret_cast<const uint64_t*>(buf);
				if (len == 24 && magic == UINT64_C(0x3a44493a4f494d44)) { // "DMIO:ID:"
					return new GuidPartitionDevice(data, parseGuid(buf, 8));
				} else if (magic == UINT64_C(0x005c003f003f005c) // "\??\"
						|| magic == UINT64_C(0x005f003f003f005f)) { // "_??_"
					if (len >= (36 + 2) * 2) {
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

						if (instancePath[instancePath.size() - 1] == '\\') {
							instancePath.resize(instancePath.size() - 1);
						}

						return new GenericDevice(data, instancePath, intfGuid);
					}
				}
			}
			return new RawDevice(data);
		}

		struct Value
		{
			Value()
			{
				val = { 0 };
			}

			~Value()
			{
				free(val.key);
				free(val.value);
			}

			hive_set_value* operator->() {
				return &val;
			}

			void setKey(const string& key)
			{
				free(val.key);
				val.key = strdup(key.c_str());
			}

			hive_set_value val;
		};

		void getValue(hive_h* hive, hive_node_h node, const string& name, Value& out)
		{
			hive_value_h handle = hivex_node_get_value(hive, node, name.c_str());
			if (!handle) {
				throw ErrnoException("hivex_node_get_value");
			}

			out->value = hivex_value_value(hive, handle, &out->t, &out->len);
			if (!out->value) {
				throw ErrnoException("hivex_value_value");
			}

			out->key = strdup(name.c_str());
			if (!out->key) {
				throw ErrnoException("strdup");
			}
		}
	}

	MountedDevices::MountedDevices(const char* filename, bool writable)
	{
		_hive = hivex_open(filename, writable ? HIVEX_OPEN_WRITE : 0);
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

	vector<unique_ptr<Device>> MountedDevices::list(int flags) const
	{
		hive_value_h *values = hivex_node_values(_hive, _node);
		if (!values) {
			throw ErrnoException("hivex_node_values");
		}

		vector<unique_ptr<Device>> devices;

		for (; *values; ++values) {
			int letter = 0;
			string key(toString(hivex_value_key(_hive, *values)));

			if (key.find("\\DosDevices\\") == string::npos) {
				// do something here
			} else if (key.size() != 14 || key[key.size() - 1] != ':') {
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

			unique_ptr<Device> device(createDevice(toString(buf, len)));

			if (letter) {
				device->_name = DeviceName::letter(letter);
			} else {
				if (!(flags & LIST_WITHOUT_LETTER)) {
					continue;
				}

				if (key.find("\\??\\Volume{") != 0) {
					throw new runtime_error("Invalid key " + key);
				}

				device->_name = DeviceName::volume(key.substr(11, 36));
			}

			devices.push_back(move(device));
		}

		return devices;
	}

	void MountedDevices::swap(char a, char b)
	{
		Value aVal, bVal;

		getValue(_hive, _node, DeviceName::letter(a).key(), aVal);
		getValue(_hive, _node, DeviceName::letter(b).key(), bVal);

		// The logical thing to do would be to rename the values,
		// but hivex does not support this, so we read the 
		// values, swap the keys, and write them back to the hive.

		::swap(aVal->key, bVal->key);

		if (hivex_node_set_value(_hive, _node, &aVal.val, 0) != 0) {
			throw ErrnoException("hivex_node_set_value");
		}

		if (hivex_node_set_value(_hive, _node, &bVal.val, 0) != 0) {
			throw ErrnoException("hivex_node_set_value");
		}

		if (hivex_commit(_hive, NULL, 0) != 0) {
			throw ErrnoException("hivex_commit");
		}
	}

	void MountedDevices::change(char from, char to)
	{
		Value val;
		getValue(_hive, _node, DeviceName::letter(from).key(), val);

		string key(DeviceName::letter(to).key());

		hive_value_h handle = hivex_node_get_value(_hive, _node, key.c_str());
		if (handle) {
			hive_type t;
			size_t len;

			if (hivex_value_type(_hive, handle, &t, &len) != 0) {
				throw ErrnoException("hivex_value_type");
			}

			if (len != 0) {
				throw invalid_argument(string("Drive letter ") 
						+ to + ": is already taken");
			}

			// Zero length means removed (by us) - Windows will boot
			// happily with a zero-length \\DosDevices\\X: entry.
		}

		val.setKey(key);

		if (hivex_node_set_value(_hive, _node, &val.val, 0) != 0) {
			throw ErrnoException("hivex_node_set_value");
		}

		if (hivex_commit(_hive, NULL, 0) != 0) {
			throw ErrnoException("hivex_commit");
		}

		remove(from);
	}

	void MountedDevices::remove(char letter)
	{
		Value val;
		getValue(_hive, _node, DeviceName::letter(letter).key(), val);

		// hivex does not support deleting values, so just clear it
		val->len = 0;

		if (hivex_node_set_value(_hive, _node, &val.val, 0) != 0) {
			throw ErrnoException("hivex_node_set_value");
		}

		if (hivex_commit(_hive, NULL, 0) != 0) {
			throw ErrnoException("hivex_commit");
		}
	}
}
