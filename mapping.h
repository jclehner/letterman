#ifndef LETTERMAN_DEVICES_H
#define LETTERMAN_DEVICES_H
#include <stdint.h>
#include <iostream>
#include <string>

namespace letterman {

	class MappingName
	{
		public:

		MappingName()
		: _letter(0), _guid("") {}

		static MappingName letter(char letter) {
			return MappingName(letter, "");
		}

		static MappingName volume(const std::string& guid) {
			return MappingName(0, guid);
		}

		std::string key() const;

		friend std::ostream& operator<<(std::ostream& os, const MappingName& name)
		{
			if (name._letter) {
				os << name._letter << ":";
			} else if (!name._guid.empty()) {
				os << "Volume{" << name._guid << "}";
			} else {
				os << "(Invalid)";
			}

			return os;
		}

		private:
		MappingName(char letter, const std::string& guid);

		char _letter;
		std::string _guid;
	};

	class Mapping
	{
		public:

		static const std::string kOsNameNotAttached;
		static const std::string kOsNameUnknown;

		Mapping() {}
		virtual ~Mapping() {}

		const MappingName& name() const {
			return _name;
		}

		virtual std::string toString(int padding) const = 0;
		virtual std::string osDeviceName() const
		{ return kOsNameUnknown; }

		friend class MountedDevices;

		private:
		MappingName _name;
	};

	class RawMapping : public Mapping 
	{
		public:
		RawMapping(const std::string& data)
		: _data(data) {}

		virtual ~RawMapping() {}

		virtual std::string toString(int padding) const override;

		private:
		std::string _data;
	};

	class MbrPartitionMapping : public Mapping
	{
		public:
		MbrPartitionMapping(uint32_t disk, uint64_t offset)
		: _disk(disk), _offset(offset) {}

		virtual ~MbrPartitionMapping() {}

		virtual std::string toString(int padding) const override;
		virtual std::string osDeviceName() const override;

		private:
		uint32_t _disk;
		uint64_t _offset;
	};

	class GuidPartitionMapping : public Mapping
	{
		public:
		GuidPartitionMapping(const std::string& guid)
		: _guid(guid)
		{}

		virtual ~GuidPartitionMapping() {}

		virtual std::string toString(int padding) const override;
		virtual std::string osDeviceName() const override;

		private:
		std::string _guid;
	};

	class GenericMapping : public Mapping
	{
		public:
		GenericMapping(const std::string& path, 
				const std::string& guid)
		: _path(path), _guid(guid)
		{}

		virtual ~GenericMapping() {}

		virtual std::string toString(int padding) const override;

		private:
		std::string _path;
		std::string _guid;
	};
}
#endif
