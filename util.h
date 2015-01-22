#ifndef LETTERMAN_UTIL_H
#define LETTERMAN_UTIL_H
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <string>
#include <cctype>

namespace letterman {
	namespace util {
		template<typename T> std::string toString(const T& t)
		{
			std::ostringstream ostr;
			ostr << t;
			return ostr.str();
		}

		template<typename T> T fromString(const std::string& str, 
				std::ios_base::fmtflags mask = std::ios::dec)
		{
			T t;
			std::istringstream istr(str);
			istr.setf(mask);

			if (istr >> t) {
				return t;
			}

			throw std::invalid_argument("Failed to convert " + str);
		}

		inline void capitalize(std::string& str)
		{
			std::transform(str.begin(), str.end(), str.begin(), ::toupper);
		}

		std::ostream& hexdump(std::ostream& os, const std::string& data,
				unsigned padding = 0);

		inline std::ostream& hexdump(std::ostream& os, const void* data, size_t len,
				unsigned padding = 0)
		{
			return hexdump(os, std::string(static_cast<const char*>(data), len), 
					padding);
		}


		template<typename T> using UniquePtrWithDeleter = 
			std::unique_ptr<T, std::function<void(T*)>>;
	}
}
#endif
