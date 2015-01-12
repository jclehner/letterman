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
	MountedDevices(const char* filename, bool writable = false);
	~MountedDevices();

	static const int LIST_WITHOUT_LETTER = 1;

	std::unique_ptr<Device> find(const DeviceName& name) const;
	std::vector<std::unique_ptr<Device>> list(int flags = 0) const;

	void swap(const DeviceName& a, const DeviceName& b);

	private:
	hive_h *_hive;
	hive_node_h _node;
};

}
#endif
