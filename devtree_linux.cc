#ifdef __linux__
#include <libudev.h>
#include <algorithm>
#include <mntent.h>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <map>
#include "exception.h"
#include "devtree.h"
#include "util.h"
using namespace std;

namespace letterman {
	namespace {
		static const char* PROPS[] = {
			"DEVNAME", "ID_PART_ENTRY_OFFSET", "ID_PART_ENTRY_NUMBER", "ID_FS_LABEL",
			"ID_FS_LABEL_ENC", "UDISKS_PARTITION_NUMBER", "UDISKS_PARTITION_OFFSET",
			"ID_DRIVE_FLOPPY", "MAJOR", "MINOR", "ID_SERIAL", "ID_SERIAL_SHORT",
			"ID_PART_ENTRY_DISK", "DEVTYPE", "ID_PART_TABLE_UUID", "ID_PART_ENTRY_UUID",
			"ID_FS_TYPE", "ID_MODEL"
		};


		void capitalize(string& str)
		{
			transform(str.begin(), str.end(), str.begin(), ::toupper);
		}

		string getMountPoint(const string& device)
		{
			FILE* fp = setmntent("/proc/mounts", "r");
			if (!fp) fp = setmntent("/etc/mtab", "r");
			if (!fp) throw ErrnoException("setmntent");

			struct mntent* mnt;
			while ((mnt = getmntent(fp))) {
				if (device == mnt->mnt_fsname) {
					return mnt->mnt_dir;
				}
			}

			return "";
		}
	}

	const string DevTree::kPropDevice = "DEVNAME";
	const string DevTree::kPropMajor = "MAJOR";
	const string DevTree::kPropMinor = "MINOR";
	const string DevTree::kPropFsLabel = "ID_FS_LABEL";
	const string DevTree::kPropPartUuid = "ID_PART_ENTRY_UUID";
	const string DevTree::kPropModel = "ID_MODEL";

	const string DevTree::kPropMountPoint = "kPropMountPoint";
	const string DevTree::kPropMbrId = "ID_PART_TABLE_UUID";
	const string DevTree::kPropPartOffsetBlocks = "ID_PART_ENTRY_OFFSET"; 
	const string DevTree::kPropPartOffsetBytes = "UDISKS_PARTITION_OFFSET";

	map<string, Properties> DevTree::getAllDevices()
	{
		static map<string, Properties> entries;
		if (!entries.empty()) return entries;

		util::UniquePtrWithDeleter<udev> udev(udev_new(),
				[] (struct udev* p) { udev_unref(p); });
		if (!udev) {
			throw ErrnoException("udev_new");
		}

		util::UniquePtrWithDeleter<udev_enumerate> enumerate(
				udev_enumerate_new(udev.get()),
				[] (udev_enumerate* p) { udev_enumerate_unref(p); });

		udev_enumerate_add_match_subsystem(enumerate.get(), "block");
		udev_enumerate_scan_devices(enumerate.get());

		udev_list_entry *devices, *dev_list_entry;
		devices = udev_enumerate_get_list_entry(enumerate.get());


		udev_list_entry_foreach(dev_list_entry, devices) {

			const char* path = udev_list_entry_get_name(dev_list_entry);
			util::UniquePtrWithDeleter<udev_device> dev(
					udev_device_new_from_syspath(udev.get(), path),
					[] (udev_device* p) { udev_device_unref(p); });

			Properties props;

			for (const char* key : PROPS) {
				const char* prop = udev_device_get_property_value(dev.get(), key);
				props[key] = prop ? prop : "";
			}

			if (props["ID_DRIVE_FLOPPY"] == "1") continue;

			if (!props.empty()) {
				if (isDisk(props)) {
					props[kPropDiskId] = props["MAJOR"] + ":" + props["MINOR"];
				} else if (isPartition(props)) {
					props[kPropDiskId] = props["ID_PART_ENTRY_DISK"];
					props[kPropIsNtfs] = (props["ID_FS_TYPE"] == "ntfs" ? "1" : "0");
					props[kPropMountPoint] = getMountPoint(props["DEVNAME"]);
				} else {
					continue;
				}

				capitalize(props[kPropPartUuid]);

				entries[props["DEVNAME"]] = props;
			}
		}

		return entries;
	}

	bool DevTree::isDiskOrPartition(const Properties& props, bool isDisk)
	{
		auto i = props.find("DEVTYPE");
		return i != props.end() && i->second == (isDisk ? "disk" : "partition");
	}
}
#endif
