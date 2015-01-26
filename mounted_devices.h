#ifndef LETTERMAN_MOUNTED_DEVICES_H
#define LETTERMAN_MOUNTED_DEVICES_H
#include <memory>
#include <vector>
#include <string>
#include <hivex.h>
#include "mapping.h"

namespace letterman {

class MountedDevices
{
	public:
	MountedDevices(const std::string& filename, bool writable = false);
	~MountedDevices();

	static const int LIST_WITHOUT_LETTER = 1;

	Mapping::Ptr find(const MappingName& name) const;
	std::vector<Mapping::Ptr> list(int flags = 0) const;

	void swap(char a, char b);
	void change(char from, char to);
	void remove(char letter);
	void disable(char letter);
	void enable(char letter);

	void add(char a, const void* data, size_t len);

	private:
	hive_h *_hive;
	hive_node_h _node;
};

}
#endif
