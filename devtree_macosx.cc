#ifdef __APPLE__
#include <DiskArbitration/DADisk.h>
#define KERNEL
#include <IOKit/storage/IOBlockStorageDevice.h>
#undef KERNEL
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOKitLib.h>
#include <libkern/OSTypes.h>
#include <stdexcept>
#include <iostream>
#include "exception.h"
#include "devtree.h"

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

			throw ErrnoException("CFStringGetCString");
		}

		string fromStringRef(const void* str, bool release = false)
		{
			return fromStringRef(static_cast<CFStringRef>(str), release);
		}

		static const string kPropIsDisk = fromStringRef(
				kDADiskDescriptionMediaWholeKey, false);

		static const string kPropBsdUnit = fromStringRef(
				kDADiskDescriptionMediaBSDUnitKey);

		const string kPropFsType = fromStringRef(
				kDADiskDescriptionVolumeKindKey, false);

		const string kPropDeviceModelMounted = fromStringRef(
				kDADiskDescriptionDeviceModelKey, false);

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

	const string DevTree::kPropModel = "kPropModel";
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

		// First pass: get mounted device info

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

				if (isPartition(props)) {
					props[kPropIsNtfs] = (props[kPropFsType] == "ntfs" ? "1" : "0");
					if (props[kPropMountPoint].find("file://") == 0) {
						props[kPropMountPoint] = props[kPropMountPoint].substr(7);
					} else {
						props[kPropMountPoint] = "";
					}
				}

				ret[props[kPropDevice]] = props;
			}

			props.clear();

			CFRelease(disk);
			IOObjectRelease(device);
		}

		CFRelease(session);

		// Second pass: get optical drive device info

		kr = IOServiceGetMatchingServices(
				kIOMasterPortDefault,
				IOServiceMatching(kIOBlockStorageDeviceClass),
				&iter);

		if (kr != KERN_SUCCESS)
			throw ErrnoException("IOServiceGetMatchingServices");

		while ((device = IOIteratorNext(iter))) {
			io_string_t path;

			kr = IORegistryEntryGetPath(device, kIOServicePlane, path);
			if (kr != KERN_SUCCESS) continue;

			CFTypeRef prop = IORegistryEntryCreateCFProperty(
					device, CFSTR(kIOBlockStorageDeviceTypeKey),
					kCFAllocatorDefault, 0);

			if (prop) {
				string deviceType(toString(prop, false));
				if (deviceType != "DVD" && deviceType != "CD" 
					&& deviceType != "BD") {
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


			string vendor(toString(val));

			val = CFDictionaryGetValue(dict, CFSTR(kIOPropertyProductNameKey));
			if (!val) {
				continue;
			}

			string product(toString(val));

			ret[path][kPropModel] = vendor + product;

			IOObjectRelease(device);
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
