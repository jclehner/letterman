#ifdef __APPLE__
#include <DiskArbitration/DADisk.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOKitLib.h>
#include <stdexcept>
#include "exception.h"
#include "devtree.h"
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

		static const string kPropIsDisk = fromStringRef(
				kDADiskDescriptionMediaWholeKey, false);

		static const string kPropBsdUnit = fromStringRef(
				kDADiskDescriptionMediaBSDUnitKey);

		const string kPropFsType = fromStringRef(
				kDADiskDescriptionVolumeKindKey, false);

		string toString(CFTypeRef ref)
		{
			CFTypeID id = CFGetTypeID(ref);

			if (id == CFStringGetTypeID()) {
				return fromStringRef(static_cast<CFStringRef>(ref), false);
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
		io_service_t service;

		while ((service = IOIteratorNext(iter))) {
			DADiskRef disk = DADiskCreateFromIOMedia(
					kCFAllocatorDefault, session, service);

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
				}

				ret[props[kPropDevice]] = props;
			}

			props.clear();

			CFRelease(disk);
		}

		CFRelease(session);

		return ret;
	}

	bool DevTree::isDiskOrPartition(const Properties& props, bool isDisk)
	{
		auto i = props.find(kPropIsDisk);
		return i != props.end() && i->second == (isDisk ? "1" : "0");
	}
}
#endif
