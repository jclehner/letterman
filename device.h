#ifndef LETTERMAN_DEVICES_H
#define LETTERMAN_DEVICES_H
#include <stdint.h>
#include <iostream>
#include <string>

namespace letterman {

	class DeviceSelector
	{
		public:

		DeviceSelector()
		: _letter(0), _guid("") {}

		static DeviceSelector letter(char letter) {
			return DeviceSelector(letter, "");
		}

		static DeviceSelector volume(const std::string& guid) {
			return DeviceSelector(0, guid);
		}

		bool matches(const std::string& key) const;

		friend std::ostream& operator<<(std::ostream& os, const DeviceSelector& selector)
		{
			if (selector._letter) {
				os << "Drive " << selector._letter << ":";
			} else if (!selector._guid.empty()) {
				os << "Volume " << selector._guid;
			} else {
				os << "(Invalid)";
			}

			return os;
		}

		private:
		DeviceSelector(char letter, const std::string& guid);

		char _letter;
		std::string _guid;
	};

	class Device
	{
		public:
		virtual ~Device() {}

		const DeviceSelector& selector() const {
			return _selector;
		}

		virtual std::string toString(int padding) const = 0;
		virtual std::string toRawString(int padding) const = 0;

		friend class MountedDevices;

		private:
		DeviceSelector _selector;
	};

	class RawDevice : public Device 
	{
		public:
		RawDevice(const std::string& data)
		: _data(data) {}

		virtual ~RawDevice() {}

		virtual std::string toString(int padding) const;

		inline virtual std::string toRawString(int padding) const {
			return RawDevice::toString(padding);
		}

		private:
		std::string _data;
	};

	class MbrPartitionDevice : public RawDevice
	{
		public:
		MbrPartitionDevice(const std::string& data, uint32_t disk, uint64_t offset)
		: RawDevice(data), _disk(disk), _offset(offset) {}

		virtual ~MbrPartitionDevice() {}

		virtual std::string toString(int padding) const;

		private:
		uint32_t _disk;
		uint64_t _offset;
	};

	class GuidPartitionDevice : public RawDevice
	{
		public:
		GuidPartitionDevice(const std::string& data, const std::string& guid)
		: RawDevice(data), _guid(guid)
		{}

		virtual ~GuidPartitionDevice() {}

		virtual std::string toString(int padding) const;

		private:
		std::string _guid;
	};

	class GenericDevice : public RawDevice
	{
		public:
		GenericDevice(const std::string& data, const std::string& path, 
				const std::string& guid)
		: RawDevice(data), _path(path), _guid(guid)
		{}

		virtual ~GenericDevice() {}

		virtual std::string toString(int padding) const;

		private:
		std::string _path;
		std::string _guid;
	};
}
#endif
