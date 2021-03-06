#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <cctype>
#include "mounted_devices.h"
#include "exception.h"
#include "endian.h"
#include "util.h"
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
		// where the second byte is non-zero (or the first
		// byte a non-7-bit char)
		string fromWstring(string wstr)
		{
			if (wstr.size() % 2) {
				throw invalid_argument("wstr");
			}

			string ret;

			for(string::size_type i = 0; i < wstr.size(); i += 2) {
				ret += (wstr[i + 1] || wstr[i] & 0x80) ? '?' : wstr[i];
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

		Mapping* createMapping(const string& data)
		{
			const char* buf = data.c_str();
			size_t len = data.size();

			if (len == 12) {
				uint32_t disk = le32toh(*reinterpret_cast<const uint32_t*>(buf));
				uint64_t offset = le64toh(*reinterpret_cast<const uint64_t*>(buf + 4));
				return new MbrPartitionMapping(disk, offset);
			} else if (len >= 8) {
				uint64_t magic = *reinterpret_cast<const uint64_t*>(buf);
				if (len == 24 && magic == UINT64_C(0x3a44493a4f494d44)) { // "DMIO:ID:"
					return new GuidPartitionMapping(parseGuid(buf, 8));
				} else if (magic == UINT64_C(0x005c003f003f005c) // "\??\"
						|| magic == UINT64_C(0x005f003f003f005f)) { // "_??_"
					if (len >= (36 + 2) * 2) {
						string bytes(fromWstring(data));

						// Data is composed of the "Mapping Instance Path", with an
						// appended GUID specifying the "Mapping Interface"
						// (http://msdn.microsoft.com/en-us/library/windows/hardware/ff545813%28v=vs.85%29.aspx)
						// Note that the GUID is surrounded by {}
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

						return new GenericMapping(instancePath, intfGuid);
					}
				}
			}
			return new RawMapping(data);
		}

		struct Value
		{
			Value()
			: freeKey(true), freeValue(true)
			{
				val = { 0 };
			}

			~Value()
			{
				if (freeKey) free(val.key);
				if (freeValue) free(val.value);
			}

			hive_set_value* operator->() {
				return &val;
			}

			hive_set_value* operator*() {
				return &val;
			}

			void setKey(const string& key)
			{
				if (freeKey) free(val.key);
				val.key = strdup(key.c_str());
			}

			bool freeKey;
			bool freeValue;

			hive_set_value val;
		};

		bool getValue(hive_h* hive, hive_node_h node, const string& name, Value& out)
		{
			hive_value_h handle = hivex_node_get_value(hive, node, name.c_str());
			if (!handle) {
				return false;
			}

			out->value = hivex_value_value(hive, handle, &out->t, &out->len);
			if (!out->value) {
				throw ErrnoException("hivex_value_value");
			}

			out->key = strdup(name.c_str());
			if (!out->key) {
				throw ErrnoException("strdup");
			}

			return true;
		}

		void getValue(hive_h* hive, hive_node_h node, char letter, Value& out)
		{
			if (!getValue(hive, node, MappingName::letter(letter).key(), out)) {
				throw UserFault(string("Letter is not mapped to any volume: ") + letter + ":");
			}
		}

		void requireDriveLetterNotTaken(hive_h* hive, hive_node_h node, char letter)
		{
			string key(MappingName::letter(letter).key());
			hive_value_h handle = hivex_node_get_value(hive, node, key.c_str());
			if (handle) {
				hive_type t;
				size_t len;

				if (hivex_value_type(hive, handle, &t, &len) != 0) {
					throw ErrnoException("hivex_value_type");
				}

				if (len != 0) {
					throw UserFault(string("Drive letter ") 
							+ letter + ": is already taken");
				}

				// Zero length means removed (by us) - Windows will boot
				// happily with a zero-length \\DosMappings\\X: entry.
			}
		}
	}

	MountedDevices::MountedDevices(const string& filename, bool writable)
	{
		_hive = hivex_open(filename.c_str(), writable ? HIVEX_OPEN_WRITE : 0);
		if (!_hive) {
			throw ErrnoException("hivex_open: " + filename);
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

	vector<Mapping::Ptr> MountedDevices::list(int flags) const
	{
		hive_value_h *values = hivex_node_values(_hive, _node);
		if (!values) {
			throw ErrnoException("hivex_node_values");
		}

		vector<Mapping::Ptr> devices;

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

			if (!len) continue;

			Mapping::Ptr device(createMapping(toString(buf, len)));

			if (letter) {
				device->_name = MappingName::letter(letter);
			} else {
				if (!(flags & LIST_WITHOUT_LETTER)) {
					continue;
				}

				if (key.find("\\??\\Volume{") != 0) {
					throw new runtime_error("Invalid key " + key);
				}

				device->_name = MappingName::volume(key.substr(11, 36));
			}

			devices.push_back(move(device));
		}

		return devices;
	}

	void MountedDevices::swap(char a, char b)
	{
		Value aVal, bVal;

		getValue(_hive, _node, a, aVal);
		getValue(_hive, _node, b, bVal);

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
		requireDriveLetterNotTaken(_hive, _node, to);

		Value val;
		getValue(_hive, _node, from, val);

		string key(MappingName::letter(to).key());

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
		getValue(_hive, _node, letter, val);

		// hivex does not support deleting values, so just clear it
		val->len = 0;

		if (hivex_node_set_value(_hive, _node, &val.val, 0) != 0) {
			throw ErrnoException("hivex_node_set_value");
		}

		if (hivex_commit(_hive, NULL, 0) != 0) {
			throw ErrnoException("hivex_commit");
		}
	}

	void MountedDevices::add(char letter, const void* data, size_t len)
	{
		requireDriveLetterNotTaken(_hive, _node, letter);

		Value val;
		val->key = strdup(MappingName::letter(letter).key().c_str());
		val->value = static_cast<char*>(const_cast<void*>(data));
		val.freeValue = false;
		val->len = len;
		val->t = hive_t_REG_BINARY;

		if (hivex_node_set_value(_hive, _node, &val.val, 0) != 0) {
			throw ErrnoException("hivex_node_set_value");
		}

		if (hivex_commit(_hive, NULL, 0) != 0) {
			throw ErrnoException("hivex_commit");
		}
	}
}
