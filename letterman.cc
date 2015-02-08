#include <iostream>
#include <sstream>
#include <string>
#include "mounted_devices.h"
#include "hive_crawler.h"
#include "exception.h"
#include "devtree.h"
#include "endian.h"
#include "util.h"
using namespace std;
using namespace letterman;

namespace {
	void requireDriveLetter(const string& arg)
	{
		bool invalid = false;

		switch (arg.size()) {
			case 2:
				invalid |= arg[1] != ':';
				// fall through
			case 1:
				invalid |= (arg[0] < 'A' || arg[0] > 'Z');
				break;

			default:
				invalid = true;
		}

		if (invalid) {
			throw UserFault("Not a drive letter: " + arg);
		}
	}

	void requireArgCount(int argc, int n)
	{
		if (argc - 1 != n) {
			ostringstream ostr;
			ostr << "Action requires " << n << " argument";
			ostr << (n == 1 ? "" : "s") << ", got " << (n - 1);
			throw UserFault(ostr.str());
		}
	}

	string getHiveFromArgs(int argc, char **argv, int& index)
	{
		string opt(argv[1]);

		if (opt == "--probe" /*|| argv[1][0] != '-'*/) {
			set<WindowsInstall> installs(getAllWindowsInstalls());
			if (installs.empty()) {
				throw UserFault(
						"No Windows installations found. Specify one manually\n"
						"with --sysdrive, --sysroot, --sysdir or --hive. Use\n"
						"--probe to try auto-detection (needs root).\n");
			} else if (installs.size() > 1) {
				ostringstream ostr;
				ostr << "Multiple windows installations found. Specify manually with" << endl;
				for (auto& install : installs) {
					ostr << "  " << install << endl;
				}
				throw UserFault(ostr.str());
			}

			index += 1;
			return hiveFromSysDrive(installs.begin()->path);
		}

		if (opt.substr(0, 2) == "--") {
			if (argc < 3) {
				throw UserFault(opt + " requires an argument");
			}

			string arg(argv[2]);

			index += 2;

			if (opt == "--sysdrive") {
				return hiveFromSysDrive(arg);
			} else if (opt == "--sysroot") {
				return hiveFromSysRoot(arg);
			} else if (opt == "--sysdir") {
				return hiveFromSysDir(arg);
			} else if (opt == "--hive") {
				return arg;
			}

			throw UserFault("Unknown option " + opt);
		}

		throw UserFault(
				"You must specify a Windows installation manually using\n"
				"--sysdrive, --sysroot, --sysdir or --hive. Use\n"
				"--probe to try auto-detection (needs root).\n");
	}

	[[noreturn]] void printUsageAndDie()
	{
		cerr << "usage: letterman [action] [arguments ...]" << endl;
		cerr << "actions: list, swap, change, remove" << endl;
		exit(1);
	}

}

int main(int argc, char **argv)
{
	try {
		if (argc < 2) printUsageAndDie();

		int i = 1;
		string hive(getHiveFromArgs(argc, argv, i));

		argc -= i;

		if (!argc) printUsageAndDie();

		string action(argv[i]);
		string arg1(argc >= 2 ? argv[i + 1] : "");
		string arg2(argc >= 3 ? argv[i + 2] : "");
		string arg3(argc >= 4 ? argv[i + 3] : "");

		if (action == "swap" || action == "change") {
			requireArgCount(argc, 2);
			requireDriveLetter(arg1);
			requireDriveLetter(arg2);

			char a = arg1[0];
			char b = arg2[0];

			MountedDevices md(hive, true);

			if (action == "swap") {
				md.swap(a, b);
			} else if (action == "change") {
				md.change(a, b);
			}
		} else if (action == "remove") {
			requireArgCount(argc, 1);
			requireDriveLetter(arg1);

			MountedDevices (hive, true).remove(arg1[0]);
		} else if (action == "list") {

			for (auto&& mapping : MountedDevices(hive).list()) {
				string device(mapping->osDeviceName());

				cout << mapping->name() << "  ";

				if (device == Mapping::kOsNameUnknown) {
					cout << "? " << mapping->toString(0);
				} else if (device == Mapping::kOsNameNotAttached) {
					cout << "- " << mapping->toString(0);
				} else {
					cout << "* " << mapping->osDeviceName();
				}

				cout << endl;
			}
#if 1
		} else if (action == "add") {
			requireArgCount(argc, 3);
			requireDriveLetter(arg2);

			if (arg1 == "mbr") {
				requireArgCount(argc, 4);

				struct Entry
				{
					uint32_t disk;
					uint64_t offset;
				} __attribute__((packed));

				Entry e;
				e.disk = htole32(util::fromString<uint32_t>(arg2, ios::hex));
				e.offset = htole64(util::fromString<uint64_t>(arg3));

				util::hexdump(cout, &e, 12, 4) << endl;

				MountedDevices(hive, true).add(arg2[0], &e, 12);
			} else if (arg1 == "raw") {
				string wstr;
				for (char c : arg3) {
					wstr += c;
					wstr += '\0';
				}

				util::hexdump(cout, wstr.c_str(), wstr.size(), 4) << endl;
				MountedDevices(hive, true).add(arg2[0], wstr.c_str(), wstr.size());
			} else if (arg1 == "partition") {
				
				Properties criteria = {{ DevTree::kPropDeviceMountable, arg3 }};
				map<string, Properties> result(DevTree::getPartitions(criteria));
				if (result.empty()) throw UserFault("No such partition: " + arg3);

				Properties props = result.begin()->second;

				uint64_t offsetMult = 1;
				string offsetStr = props[DevTree::kPropPartOffsetBlocks];
				if (!offsetStr.empty()) {
					offsetMult = 512;
				} else {
					offsetStr = props[DevTree::kPropPartOffsetBytes];
				}

				if (offsetStr.empty()) {
					throw UserFault("Failed to determine partition offset; must specify manually");
				}

				// Get the disk with the corresponding kPropDiskId
				criteria = {{ DevTree::kPropDiskId, props[DevTree::kPropDiskId] }};
				result = DevTree::getDisks(criteria);

				if (result.empty()) {
					throw UserFault("Failed to determine hosting disk of partition " + arg3);
				}

				props = result.begin()->second;

				string mbrIdStr = props[DevTree::kPropMbrId];
				if (mbrIdStr.empty()) {
					throw UserFault("Failed to determine MBR disk id of partition " + arg3);
				}

				struct Entry
				{
					uint32_t disk;
					uint64_t offset;
				} __attribute__((packed));

				Entry e;

				e.disk = htole32(util::fromString<uint32_t>(mbrIdStr, ios::hex));
				e.offset = htole64(offsetMult * util::fromString<uint64_t>(offsetStr));

				MountedDevices(hive, true).add(arg2[0], &e, sizeof(e));

			} else {
				throw UserFault("Unknown type: " + arg1);
			}
		} else if (action == "dump") {
			requireArgCount(argc, 1);

			map<string, Properties> data;

			if (arg1 == "partitions") data = DevTree::getPartitions();
			else if (arg1 == "disks") data = DevTree::getDisks();

			for (auto& e : data) {
				cout << e.first << "\t" << e.second[DevTree::kPropHardware] << endl;
#if 1
				for (auto& props : e.second) {
					cout << "  " << props.first << "=" << props.second << endl;
				}
#endif
			}
#endif
		} else {
			cerr << action << ": unknown action" << endl;
			return 1;
		}
	} catch (const UserFault& uf) {
		cerr << uf.what() << endl;
		return 1;
	} catch (const std::exception& e) {
		cerr << typeid(e).name() << endl;
		cerr << e.what() << endl;
		return 1;
	}
}
