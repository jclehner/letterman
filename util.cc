#include "util.h"
using namespace std;

namespace letterman {
	namespace util {
		ostream& hexdump(ostream& os, const string& data, unsigned padding)
		{
			ios_base::fmtflags saved = os.flags();

			os << hex << setfill('0');

			for (size_t i = 0; i < data.size(); i += 16) {
				if (i != 0) os << endl;
				os << string(padding, ' ') << setw(4) << i << " ";
				string ascii;
				for (size_t k = 0; k != 16; ++k) {
					if (k == 8) os << " ";
					if (i + k < data.size()) {
						int c = data[i + k] & 0xff;
						os << " " << setw(2) << c;
						ascii += isprint(c) ? c : '.';
					} else {
						os << "   ";
					}
				}

				os << "  |" << ascii << "|";
			}

			os.flags(saved);
			return os;
		}

		string& replaceAll(string& str, char from, char to)
		{
			string::size_type i;
			while ((i = str.find(from)) != string::npos) {
				str[i] = to;
			}

			return str;
		}

		string& rtrim(string& str)
		{
			string::size_type i = str.find_last_not_of(" \t\r\n");
			if (i != string::npos) str.resize(i + 1);
		}
	}
}
