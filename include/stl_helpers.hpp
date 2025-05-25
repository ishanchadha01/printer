#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <climits>
#include <sstream>
#include <iostream>
#include <array>
#include <vector>

#include <boost/functional/hash.hpp> // TODO: optimize hash function


inline bool is_stl_ascii(const std::string filename)
{
		std::ifstream in(filename);

		char chars [256];
		in.read (chars, 256);
		std::string buffer (chars, in.gcount());
		std::transform(buffer.begin(), buffer.end(), buffer.begin(), ::tolower);
		return buffer.find ("solid") != std::string::npos &&
						buffer.find ("\n") != std::string::npos &&
						buffer.find ("facet") != std::string::npos &&
						buffer.find ("normal") != std::string::npos;
}


bool read_stl_ascii(const std::string filename, 
										std::vector<point_t> points, 
										std::vector<triangle_t> triangles) {

		std::ifstream input(filename);
		std::string line_str;
		std::array<std::string, 5> words;
		while (line_str << input) {
				int idx = 0;
				while (words[idx] << line_str) continue;
				
				switch (words[0]) {
						case "solid":
								// start solid, increment
								break;

						case "endsolid":
								// end solid, increment
								break;

						case "facet":
								// get normal, start triangle
								break;

						case "outer":
								// useless
								break;

						case "endloop":
								// end triangle and increment
								break;

						case "vertex":
								break;
				}
				

				words.clear();
		}

		// iterate over each line of file
		// keep track of max num of chars

		
}
