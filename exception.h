#ifndef LETTERMAN_EXCEPTION_H
#define LETTERMAN_EXCEPTION_H
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <string>

namespace letterman {
	class ErrnoException : public std::exception
	{
		public:
		ErrnoException(const std::string& function, int errnum = errno)
		: _function(function), _errnum(errnum) {}

		virtual ~ErrnoException() throw() {}

		virtual const char *what() const throw()
		{
			std::string msg("error: " + _function);

			if (_errnum) {
				msg.append(": ");
			   	msg.append(std::strerror(_errnum));
			}

			return msg.c_str();
		}

		private:
		std::string _function;
		int _errnum;
	};

	class UserFault : public std::runtime_error
	{
		public:
		explicit UserFault(const std::string& message)
		: std::runtime_error(message) {}

		virtual ~UserFault() throw() {}
	};
}
#endif
