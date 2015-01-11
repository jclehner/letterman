#include <iostream>
#include "mounted_devices.h"
using namespace std;
using namespace letterman;

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		cerr << "usage: list2 [registry hive file]" << endl;
		return 1;
	}

	MountedDevices md(argv[1]);
	vector<Device*> devices(md.list());

	for(size_t i = 0; i != devices.size(); ++i)
	{
		cout << devices[i]->toString(0) << endl;
		cout << devices[i]->toRawString(2) << endl;
	}
}

