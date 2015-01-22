#include <iostream>
#include <sstream>
#include <string>
#include "mounted_devices.h"
#include "hive_crawler.h"
#include "exception.h"
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
			ostr << (n == 1 ? "" : "s") << ", got " << n << endl;
			throw UserFault(ostr.str());
		}
	}

	string getHiveFromArgs(int argc, char **argv, int& index)
	{
		string opt(argv[1]);

		if (opt == "--probe" /*|| argv[1][0] != '-'*/) {
			set<WindowsInstall> installs(getAllWindowsInstalls());
			if (installs.empty()) {
				cerr << "No Windows installations found. Specify manually with" <<
					"--sysdrive, --sysroot, --sysdir, " << endl;
				cerr << "or	--hive" << endl;
				exit(1);
			} else if (installs.size() > 1) {
				cerr << "Multiple windows installations found. Specify manually with" << endl;
				for (auto& install : installs) {
					cerr << "  " << install << endl;
				}
				exit(1);
			}

			index += 1;
			return hiveFromSysDrive(installs.begin()->path);
		}

		if (opt.substr(0, 2) == "--") {
			if (argc < 3) {
				cerr << opt << " requires an argument" << endl;
				exit(1);
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

			cerr << "Unknown option " << opt << endl;
			exit(1);
		}

		cerr << "You must specify a Windows installation with" <<
			"--sysdrive, --sysroot, --sysdir," << endl;
		cerr << "--hive or use --probe to use auto-detect" << endl;
		exit(1);
	}

	void printUsageAndDie() __attribute__((noreturn));
	void printUsageAndDie()
	{
		cerr << "usage: letterman [action] [arguments ...]" << endl;
		cerr << "actions: list, swap, change, remove" << endl;
		exit(1);
	}

}

int main(int argc, char **argv)
{
	if (argc < 2) printUsageAndDie();

	int i = 1;
	string hive(getHiveFromArgs(argc, argv, i));

	argc -= i;

	if (!argc) printUsageAndDie();

	string action(argv[i]);
	string arg1(argc >= 2 ? argv[i + 1] : "");
	string arg2(argc >= 3 ? argv[i + 2] : "");
	string arg3(argc >= 4 ? argv[i + 3] : "");

	try {
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
		} else if (action == "addmbr") {
			requireArgCount(argc, 3);
			requireDriveLetter(arg1);

			struct Entry
			{
				uint32_t disk;
				uint64_t offset;
			} __attribute__((packed));

			Entry e;
			e.disk = htole32(util::fromString<uint32_t>(arg2, ios::hex));
			e.offset = htole64(util::fromString<uint64_t>(arg3));

			util::hexdump(cout, &e, 12, 4) << endl;

			MountedDevices(hive, true).add(arg1[0], &e, 12);
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
