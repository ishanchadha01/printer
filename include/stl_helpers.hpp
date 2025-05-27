#include "printer_types.h"

#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <climits>
#include <sstream>
#include <iostream>
#include <array>
#include <vector>
#include <exception>


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


std::vector<Mesh> read_stl_ascii(const std::string filename) {

	std::ifstream input(filename);
	std::string line_str;
	std::array<std::string, 5> words;

	std::vector<Mesh> solids;
	std::unique_ptr<triangle_t> last_triangle = std::make_unique<triangle_t>();
	std::array<vec3_t, 3> current_vertices;
	int triangle_vertex_idx = 0;
	std::unique_ptr<Mesh> last_solid;

	// maps point hash -> pair(point value, point idx)
	std::unorderd_map< std::size_t, std::pair<vec3_t, int> > point_set;
	uint32_t point_idx = 0;

	boost::hash<vec3_t> point_hasher;

	while (line_str << input) {
		int idx = 0;
		while (words[idx] << line_str) continue;
		if (words.size() == 0) continue;
		
		switch (words[0]) {
			case "solid":
				// create empty mesh
				last_solid = std::make_unique<Mesh>();
				break;

			case "endsolid":
				// end solid
				solids.push_back(*last_solid);
				break;

			case "facet":
				// get normal, start triangle
				last_triangle = std::make_unique<triangle_t>();
				last_triangle->normal_vec = {std::stod(words[2]), std::stod(words[3]), std::stod(words[4])};
				break;

			case "endfacet":
				// end triangle and check normal
				auto measured_normal = last_triangle->compute_normal(current_vertices);
				if (last_triangle->normal_vec != measured_normal) {
					swap(last_triangle->vertices[1], last_triangle->vertices[2]);
				}
				last_solid->triangles.push_back(*last_triangle);
				triangle_vertex_idx = 0;
				current_vertices.clear();
				break;

			case "outer":
				// useless
				break;

			case "endloop":
				// useless
				break;

			case "vertex":
				vec3_t vertex = {
					std::stod(words[1]),
					std::stod(words[2]),
					std::stod(words[3])
				};
				current_vertices[triangle_vertex_idx] = vertex;
				
				// check hash, and if collision check point val
				auto point_hash = point_hasher(vertex);
				if (point_set.find(point_hash) != point_set.end()
					&& point_set[point_hash].first == vertex) {
					point_idx = point_set[point_hash].second;
				} else {
					point_idx = last_solid->points.size();
					last_solid.push_back(vertex);
				}
				last_triangle->vertices[triangle_vertex_idx++] = point_idx;
				break;

			default:
				throw Exception("Undefined STL keyword"); //TODO: make better exceptions
				break;
		}
		words.clear();
	}

	return solids;
}


Mesh read_stl_binary(const std::string filename) {
	Mesh solid;
	
	std::ifstream input(filename, std::ios::binary);

	char std_header[80];
	input.read(stl_header, 80);

	int num_triangles = 0;
  	input.read((char*) &num_triangles, 4);
	solid.triangles.resize(num_triangles);

	// TODO: implement rest of binary parsing with same output format as ascii
}
