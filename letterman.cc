#include <iostream>
#include <sstream>
#include <string>
#include "mounted_devices.h"
#include "hive_crawler.h"
#include "exception.h"
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
		}

		if (invalid) {
			throw UserFault("Not a drive letter: " + arg);
		}
	}

	void requireArgCount(int argc, int n)
	{
		if (argc - 2 != n) {
			ostringstream ostr;
			ostr << "Action requires " << n << " argument";
			ostr << (n == 1 ? "" : "s");
			throw UserFault(ostr.str());
		}
	}

}

int main(int argc, char **argv)
{
	if (argc < 2) {
		cerr << "usage: letterman [action] [arguments ...]" << endl;
		cerr << "actions: list, swap, change, remove" << endl;
		return 1;
	}

	set<WindowsInstall> installs(getAllWindowsInstalls());
	if (installs.empty()) {
		cerr << "No Windows installations found. Specify manually with" << endl;
		cerr << "--sysdrive, --sysroot" << endl;
		return 1;
	} else if (installs.size() > 1) {
		cerr << "Multiple windows installations found. Specify manually with" << endl;
		for (auto& install : installs) {
			cerr << "  " << install << endl;
		}
		return 1;
	}

	string hive(hiveFromSysDrive(installs.begin()->path));
	string action(argv[1]);
	string arg1(argc >= 3 ? argv[2] : "");
	string arg2(argc >= 4 ? argv[3] : "");

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
