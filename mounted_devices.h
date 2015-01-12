#ifndef LETTERMAN_MOUNTED_DEVICES_H
#define LETTERMAN_MOUNTED_DEVICES_H
#include <memory>
#include <vector>
#include <string>
#include <hivex.h>
#include "device.h"

namespace letterman {

class MountedDevices
{
	public:
	MountedDevices(const char* hiveFileName);
	~MountedDevices();

	static const int LIST_WITHOUT_LETTER = 1;

	std::unique_ptr<Device> find(const DeviceSelector& selector);
	std::vector<std::unique_ptr<Device>> list(int flags = 0);

	private:
	hive_h *_hive;
	hive_node_h _node;
};

}
#endif
