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

			throw invalid_argument("Unhandled type id");
		}

		map<string, Properties> getAllDevices()
		{
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

			map<string, Properties> ret;
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
					CFTypeRef keyRef = keys.get() + i;
					CFTypeRef valueRef = values.get() + i;

					props[toString(keyRef)] = toString(valueRef);

					CFRelease(keyRef);
					CFRelease(valueRef);
				}

				ret[props[DevTree::kPropDevice]] = props;
				props.clear();

				CFRelease(dict);
				CFRelease(disk);
			}

			CFRelease(session);

			return ret;
		}

		map<string, Properties> getDisksOrPartitions(
				const Properties& props, bool disks)
		{
			map<string, Properties> ret;

			for (auto& e : getAllDevices()) {
				if (e.second[kPropIsDisk] == (disks ? "1" : "0")) {
					continue;
				}

				if (DevTree::arePropsMatching(e.second, props)) {
					ret[e.first] = e.second;
				}
			}

			return ret;
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

	map<string, Properties> DevTree::getDisks(const Properties& props)
	{
		return getDisksOrPartitions(props, true);
	}

	map<string, Properties> DevTree::getPartitions(const Properties& props)
	{
		return getDisksOrPartitions(props, false);
	}
}
#endif
