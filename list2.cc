#include <iostream>
#include "mounted_devices.h"
using namespace std;
using namespace letterman;

int main(int argc, char **argv)
{
	if (argc != 2) {
		cerr << "usage: list2 [registry hive file]" << endl;
		return 1;
	}

	MountedDevices md(argv[1]);
	vector<unique_ptr<Device>> devices(md.list());

	for (size_t i = 0; i != devices.size(); ++i)
	{
		char letter = devices[i]->getLetter();
		if (!letter) {
			letter = '?';
		}

		cout << letter << ": " << devices[i]->toString(0) << endl;
		cout << devices[i]->toRawString(4) << endl;
	}
}

