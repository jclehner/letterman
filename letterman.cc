#include <iostream>
#include <string>
#include "mounted_devices.h"
using namespace std;
using namespace letterman;

namespace {
	bool isValidDriveLetter(const string& str)
	{
		if (str.empty() || str.size() > 2) {
			return false;
		} else {
			return isupper(str[0]) && (str.size() == 1 || str[1] == ':');
		}
	}
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		cerr << "usage: letterman [hive] [action] [arguments ...]" << endl;
		return 1;
	}

	string hive(argv[1]);
	string action(argv[2]);
	string arg1(argc >= 4 ? argv[3] : "");
	string arg2(argc >= 5 ? argv[4] : "");

	if (action == "swap") {
		if (arg1.empty() || arg2.empty()) {
			cerr << "action requires 2 arguments" << endl;
			return 1;
		}

		if (!isValidDriveLetter(arg1) || !isValidDriveLetter(arg2)) {
			cerr << "arguments must be drive letters" << endl;
			return 1;
		}

		MountedDevices(argv[1], true).swap(arg1[0], arg2[0]);
	} else {
		cerr << action << ": unknown action" << endl;
		return 1;
	}
}
