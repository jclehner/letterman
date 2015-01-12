#include <iostream>
#include "mounted_devices.h"
using namespace std;
using namespace letterman;

int main(int argc, char **argv)
{
	int flags = 0;

	if (argc < 2) {
		cerr << "usage: list2 [registry hive file]" << endl;
		return 1;
	} else if (argc >= 3 && string(argv[2]) == "all") {
		flags |= MountedDevices::LIST_WITHOUT_LETTER;
	}

	MountedDevices md(argv[1]);
	vector<unique_ptr<Device>> devices(md.list(flags));

	for (size_t i = 0; i != devices.size(); ++i)
	{
		cout << devices[i]->name() << endl;
		cout << devices[i]->toString(2) << endl;
		cout << devices[i]->toRawString(4) << endl;
	}
}

