#include <iostream>
#include <sstream>
#include <string>
#include "mounted_devices.h"
#include "exception.h"
using namespace std;
using namespace letterman;

namespace {
	void requireDriveLetter(const string& arg)
	{
		bool invalid = false;

		switch (arg.size()) {
			case 1:
				invalid |= !isupper(arg[0]);
				// fall through
			case 2:
				invalid |= arg[1] != ':';
				break;
		}

		if (invalid) {
			throw UserFault("Not a drive letter: " + arg);
		}
	}

	void requireArgCount(int argc, int n)
	{
		if (argc - 3 != n) {
			ostringstream ostr;
			ostr << "Action requires " << n << " argument";
			ostr << (n == 1 ? "" : "s");
			throw UserFault(ostr.str());
		}
	}

}

int main(int argc, char **argv)
{
	if (argc < 3) {
		cerr << "usage: letterman [hive] [action] [arguments ...]" << endl;
		cerr << "actions: swap, change, remove" << endl;
		return 1;
	}

	string hive(argv[1]);
	string action(argv[2]);
	string arg1(argc >= 4 ? argv[3] : "");
	string arg2(argc >= 5 ? argv[4] : "");

	try {
		if (action == "swap" || action == "change") {
			requireArgCount(argc, 2);
			requireDriveLetter(arg1);
			requireDriveLetter(arg2);

			char a = arg1[0];
			char b = arg2[0];

			MountedDevices md(argv[1], true);

			if (action == "swap") {
				md.swap(a, b);
			} else if (action == "change") {
				md.change(a, b);
			}
		} else if (action == "remove") {
			requireArgCount(argc, 1);
			requireDriveLetter(arg1);

			MountedDevices (argv[1], true).remove(arg1[0]);
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
