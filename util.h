#ifndef LETTERMAN_UTIL_H
#define LETTERMAN_UTIL_H
#include <algorithm>
#include <iostream>
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

		template<typename T> T fromString(const std::string& str)
		{
			T t;
			std::istringstream istr(str);

			if (istr >> t) {
				return t;
			}

			throw std::invalid_argument(std::string("Failed to convert to ") 
					+ typeid(T).name() + ": " + str);
		}

		inline void capitalize(std::string& str) {
			std::transform(str.begin(), str.end(), str.begin(), ::toupper);
		}

		template<typename T> using UniquePtrWithDeleter = 
			std::unique_ptr<T, std::function<void(T*)>>;
	}
}
#endif
