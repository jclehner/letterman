// compile with
// g++ iokit_da.cpp -o iokit_da -std=c++11 -framework DiskArbitration \
//     -framework CoreFoundation -framework IOKit

#include <DiskArbitration/DADisk.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <iostream>
#include <memory>
#include <map>
#include "exception.h"
using namespace std;

namespace letterman {
	typedef map<string, string> PropMap;

	string stringRefToString(CFStringRef str)
	{
		CFIndex length = CFStringGetLength(str);
		CFIndex size = CFStringGetMaximumSizeForEncoding(
				length, kCFStringEncodingUTF8);

		unique_ptr<char[]> buf(new char[size]);

		if (CFStringGetCString(str, buf.get(), size, kCFStringEncodingUTF8)) {
			return buf.get();
		}

		throw ErrnoException("CFStringGetCString");
	}

	string typeRefToString(CFTypeRef ref)
	{
		if (CFGetTypeID(ref) == CFStringGetTypeID()) {
			return stringRefToString((CFStringRef) ref);
		} else {
			return stringRefToString(CFCopyDescription(ref));
		}
	}

	PropMap getPropsForDevice(DADiskRef disk)
	{
		CFDictionaryRef dict = DADiskCopyDescription(disk);
		if (!dict) throw ErrnoException("DADiskCopyDescription");

		CFIndex size = CFDictionaryGetCount(dict);

		unique_ptr<CFTypeRef> keys(new CFTypeRef[size]);
		unique_ptr<CFTypeRef> values(new CFTypeRef[size]);

		CFDictionaryGetKeysAndValues(
				dict,
				(const void**) keys.get(),
				(const void**) values.get());

		PropMap props;

		for (CFIndex i = 0; i != size; ++i) {
			string key = typeRefToString(keys.get()[i]);
			string value = typeRefToString(values.get()[i]);

			props[key] = value;
		}

		return props;
	}

	PropMap getPropsForDevice(const string& device)
	{
		DASessionRef session = DASessionCreate(kCFAllocatorDefault);
		if (!session) throw ErrnoException("DASessionCreate");

		DADiskRef disk = DADiskCreateFromBSDName(
				kCFAllocatorDefault,
				session,
				device.c_str());

		if (!disk) throw ErrnoException("DADiskCreateFromBSDName");

		return getPropsForDevice(disk);
	}

	void testIoKitFind()
	{
		CFMutableDictionaryRef dict = IOServiceMatching(kIOMediaClass);

		io_iterator_t iter;
		io_service_t service;
		
		kern_return_t ret = IOServiceGetMatchingServices(
				kIOMasterPortDefault, 
				dict,
				&iter);

		if (ret != KERN_SUCCESS) {
			throw ErrnoException("IOServiceGetMatchingServices");
		}

		DASessionRef session = DASessionCreate(kCFAllocatorDefault);
		if (!session) throw ErrnoException("DASessionCreate");

		while ((service = IOIteratorNext(iter))) {

			DADiskRef disk = DADiskCreateFromIOMedia(
					kCFAllocatorDefault, session, service);

			cout << getPropsForDevice(disk)["DAMediaBSDName"] << endl;
			CFRelease(disk);
		}

		CFRelease(session);
	}
}

int main(int argc, char** argv)
{
	if (argc != 2) return 1;

	for (auto& key : letterman::getPropsForDevice(argv[1])) {
		cout << "* " << key.first << endl;
		cout << "  " << key.second << endl;
	}
}


