#ifdef __APPLE__
#include <DiskArbitration/DADisk.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOKitLib.h>
#include <libkern/OSTypes.h>
#include <stdexcept>
#include <iostream>
#include "exception.h"
#include "devtree.h"
#include "util.h"

#ifndef kIOBlockStorageDeviceTypeKey
#define kIOBlockStorageDeviceTypeKey "device-type"
#endif

using namespace std;

namespace letterman {
	namespace {

		string fromStringRef(CFStringRef str, bool release = true)
		{
			CFIndex length = CFStringGetLength(str);
			CFIndex size = CFStringGetMaximumSizeForEncoding(
					length, kCFStringEncodingUTF8);

			unique_ptr<char[]> buf(new char[size]);

			if (CFStringGetCString(str, buf.get(), size, kCFStringEncodingUTF8)) {
				if (release) CFRelease(str);
				return buf.get();
			}

			return "";

			//throw ErrnoException("CFStringGetCString");
		}

		static const string kPropIsDisk = fromStringRef(
				kDADiskDescriptionMediaWholeKey, false);

		static const string kPropBsdUnit = fromStringRef(
				kDADiskDescriptionMediaBSDUnitKey);

		const string kPropFsType = fromStringRef(
				kDADiskDescriptionVolumeKindKey, false);

		const string kPropDaDeviceModel = fromStringRef(
				kDADiskDescriptionDeviceModelKey, false);

		const string kPropDaDeviceVendor = fromStringRef(
				kDADiskDescriptionDeviceVendorKey, false);

		const string kPropDaDevicePath = fromStringRef(
				kDADiskDescriptionDevicePathKey, false);

		string toString(CFTypeRef ref, bool releaseIfString = false)
		{
			CFTypeID id = CFGetTypeID(ref);

			if (id == CFStringGetTypeID()) {
				return fromStringRef(static_cast<CFStringRef>(ref), 
						releaseIfString);
			} else if (id == CFUUIDGetTypeID()) {
				CFStringRef str = CFUUIDCreateString(
						kCFAllocatorDefault, 
						static_cast<CFUUIDRef>(ref));
				return fromStringRef(str);
			} else if (id == CFBooleanGetTypeID()) {
				Boolean value = CFBooleanGetValue(
						static_cast<CFBooleanRef>(ref));
				return value ? "1" : "0";
			} else if (id == CFNumberGetTypeID()) {
				CFStringRef str = CFStringCreateWithFormat(
						NULL, NULL, CFSTR("%@"), ref);
				return fromStringRef(str);
			}

			return fromStringRef(CFCopyDescription(ref));
		}

	}

	const string DevTree::kPropDevice = fromStringRef(
			kDADiskDescriptionMediaBSDNameKey, false);

	const string DevTree::kPropMajor = fromStringRef(
			kDADiskDescriptionMediaBSDMajorKey, false);

	const string DevTree::kPropMinor = fromStringRef(
			kDADiskDescriptionMediaBSDMinorKey, false);

	const string DevTree::kPropFsLabel = fromStringRef(
			kDADiskDescriptionMediaNameKey, false);

	const string DevTree::kPropPartUuid = fromStringRef(
			kDADiskDescriptionMediaUUIDKey, false);

	const string DevTree::kPropMountPoint = fromStringRef(
			kDADiskDescriptionVolumePathKey, false);

	const string DevTree::kPropHardware = "kPropHardware";
	const string DevTree::kPropMbrId = DevTree::kNoMatchIfSetAsPropKey;
	const string DevTree::kPropPartOffsetBlocks = DevTree::kNoMatchIfSetAsPropKey;
	const string DevTree::kPropPartOffsetBytes = DevTree::kNoMatchIfSetAsPropKey;

	map<string, Properties> DevTree::getAllDevices()
	{
		static map<string, Properties> ret;
		if (!ret.empty()) return ret;

		kern_return_t kr;
		io_iterator_t iter;

		kr = IOServiceGetMatchingServices(
				kIOMasterPortDefault,
				IOServiceMatching(kIOMediaClass),
				&iter);

		if (kr != KERN_SUCCESS)
			throw ErrnoException("IOServiceGetMatchingServices");

		DASessionRef session = DASessionCreate(kCFAllocatorDefault);
		if (!session) throw ErrnoException("DASessionCreate");

		Properties props;
		io_service_t device;

		// First pass: get fixed disk info from DiskArbitration.
		// This does not include optical drives, as these have no
		// associated device file when no medium is present.

		while ((device = IOIteratorNext(iter))) {
			DADiskRef disk = DADiskCreateFromIOMedia(
					kCFAllocatorDefault, session, device);

			CFDictionaryRef dict = DADiskCopyDescription(disk);
			CFIndex size = CFDictionaryGetCount(dict);

			unique_ptr<CFTypeRef[]> keys(new CFTypeRef[size]);
			unique_ptr<CFTypeRef[]> values(new CFTypeRef[size]);

			CFDictionaryGetKeysAndValues(
					dict,
					reinterpret_cast<const void**>(keys.get()),
					reinterpret_cast<const void**>(values.get()));

			for (CFIndex i = 0; i != size; ++i) {
				CFTypeRef keyRef = keys.get()[i];
				CFTypeRef valueRef = values.get()[i];

				props[toString(keyRef)] = toString(valueRef);

				CFRelease(keyRef);
				CFRelease(valueRef);
			}

			if (!props.empty()) {
				props[kPropDiskId] = props[kPropMajor] + ":" + props[kPropBsdUnit];
				props[kPropHardware] = props[kPropDaDeviceVendor] + props[kPropDaDeviceModel];

				if (isPartition(props)) {
					props[kPropIsNtfs] = (props[kPropFsType] == "ntfs" ? "1" : "0");
					if (props[kPropMountPoint].find("file://") == 0) {
						props[kPropMountPoint] = props[kPropMountPoint].substr(7);
					} else if (props[kPropMountPoint][0] != '/') {
						props[kPropMountPoint] = "";
					}
				}

				if (props[kPropDevice].find("disk") == 0) {
					// The key must be a readable device file,
					// which on OSX are the /dev/rdisk? files.
					props[kPropDevice].insert(0, "/dev/r");
				}

				ret[props[kPropDevice]] = props;
			}

			props.clear();

			CFRelease(disk);
			IOObjectRelease(device);
		}

		CFRelease(session);

		// Second pass: get optical drive device info from the
		// IORegistry.

		kr = IOServiceGetMatchingServices(
				kIOMasterPortDefault,
				IOServiceMatching(kIOBlockStorageDeviceClass),
				&iter);

		if (kr != KERN_SUCCESS)
			throw ErrnoException("IOServiceGetMatchingServices");

		unsigned cd = 0, dvd = 0;

		while ((device = IOIteratorNext(iter))) {
			auto cleaner(util::createCleaner(
					[&device] () { IOObjectRelease(device); }));

			io_string_t path;

			kr = IORegistryEntryGetPath(device, kIOServicePlane, path);
			if (kr != KERN_SUCCESS) continue;

			CFTypeRef prop = IORegistryEntryCreateCFProperty(
					device, CFSTR(kIOBlockStorageDeviceTypeKey),
					kCFAllocatorDefault, 0);

			string fakeDev;

			if (prop) {
				string deviceType(toString(prop, false));
				if (deviceType == "DVD" || deviceType == "BD") {
					fakeDev = "(dvd" + util::toString(dvd++) + ")";
				} else if (deviceType == "CD") {
					fakeDev = "(cdrom" + util::toString(cd++) + ")";
				} else {
					continue;
				}
			}

			prop = IORegistryEntryCreateCFProperty(
					device, CFSTR(kIOPropertyDeviceCharacteristicsKey),
					kCFAllocatorDefault, 0);

			if (!prop) {
				continue;
			}

			CFDictionaryRef dict = static_cast<CFDictionaryRef>(prop);

			const void* val = CFDictionaryGetValue(dict, CFSTR(kIOPropertyVendorNameKey));
			if (!val) {
				continue;
			}

			string hardware(toString(val));

			val = CFDictionaryGetValue(dict, CFSTR(kIOPropertyProductNameKey));
			if (!val) {
				continue;
			}

			hardware += toString(val);

			// Lookup the path in the current disk map, so we don't
			// add duplicate entries if there's a medium in the optical
			// drive.

			bool skip = false;

			for (auto& dev : ret) {
				if (dev.second[kPropDaDevicePath] == path) {
					skip = true;
					break;
				}
			}

			if (!skip) {
				ret[fakeDev][kPropIsDisk] = "1";
				ret[fakeDev][kPropHardware] = hardware;
				ret[fakeDev][kPropDaDevicePath] = path;
			}
		}

		return ret;
	}

	bool DevTree::isDiskOrPartition(const Properties& props, bool isDisk)
	{
		auto i = props.find(kPropIsDisk);
		return i != props.end() && i->second == (isDisk ? "1" : "0");
	}
}
#endif
