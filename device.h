#ifndef LETTERMAN_DEVICES_H
#define LETTERMAN_DEVICES_H
#include <stdint.h>
#include <string>

namespace letterman {

	class DeviceSelector
	{
		public:

		static DeviceSelector dosDrive(char letter) {
			return DeviceSelector(letter, "");
		}

		static DeviceSelector volume(const std::string& guid) {
			return DeviceSelector(0, guid);
		}

		bool matches(const std::string& key) const;

		private:
		DeviceSelector(char letter, const std::string& guid)
		: _letter(letter), _guid(guid) {}

		char _letter;
		std::string _guid;
	};

	class Device
	{
		public:
		Device() 
		: _letter(0) {}

		char getLetter() { 
			return _letter; 
		}

		void setLetter(char letter) {
			_letter = letter;
		}

		virtual std::string toString(int padding) = 0;
		virtual std::string toRawString(int padding) = 0;
		//virtual std::string findDevceFile() = 0;

		private:
		char _letter;
	};

	class RawDevice : public Device 
	{
		public:
		RawDevice(const std::string& data)
		: _data(data) {}

		virtual std::string toString(int padding);

		inline virtual std::string toRawString(int padding) {
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

		virtual std::string toString(int padding);

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

		virtual std::string toString(int padding);

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

		virtual std::string toString(int padding);

		private:
		std::string _path;
		std::string _guid;
	};
}
#endif
