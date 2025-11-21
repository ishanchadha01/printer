#pragma once

#include "include/containers/printer_types.hpp"
#include "include/containers/mesh.hpp"

#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdint>
#include <sstream>
#include <iostream>
#include <array>
#include <vector>
#include <exception>
#include <unordered_map>
#include <memory>


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
	std::string line_string;
	std::array<std::string, 5> words;

	std::vector<Mesh> solids;
	std::unique_ptr<triangle_t> last_triangle = std::make_unique<triangle_t>();
	std::array<vec3_t, 3> current_vertices;
	int triangle_vertex_idx = 0;
	std::unique_ptr<Mesh> last_solid;

	// maps point hash -> pair(point value, point idx)
	std::unordered_map< std::size_t, std::pair<vec3_t, int> > point_set;
	uint32_t point_idx = 0;

	boost::hash<vec3_t> point_hasher;

	while (std::getline(input, line_string)) {
		int idx = 0;
		std::istringstream line_stream(line_string);

		// stream up to 5 words, dont overrun if there's comments or weird end chars
		while (idx < static_cast<int>(words.size()) && (line_stream >> words[idx])) {
			idx++;
		}
		if (idx == 0) continue;
		
		if (words[0] == "solid") {
			// create empty mesh
			last_solid = std::make_unique<Mesh>();
		} else if (words[0] == "endsolid") {
			// end solid
			solids.emplace_back(std::move(*last_solid));
			last_solid.reset();
			point_set.clear();
		} else if (words[0] =="facet") {
			// get normal, start triangle
			last_triangle = std::make_unique<triangle_t>();
			last_triangle->normal_vec = {std::stof(words[2]), std::stof(words[3]), std::stof(words[4])};
		} else if (words[0] == "endfacet") {
			// end triangle and check normal
			auto measured_normal = last_triangle->compute_normal(current_vertices);
			if (last_triangle->normal_vec.normalize() != measured_normal.normalize()) {
				std::swap(last_triangle->vertices[1], last_triangle->vertices[2]);
			}
			last_solid->triangles.push_back(*last_triangle);
			triangle_vertex_idx = 0;
		} else if (words[0] == "vertex") {
			vec3_t vertex = {
				std::stof(words[1]),
				std::stof(words[2]),
				std::stof(words[3])
			};
			current_vertices[triangle_vertex_idx] = vertex;
			
			// check hash, and if collision check point val
			auto point_hash = point_hasher(vertex);
			if (point_set.find(point_hash) != point_set.end()
				&& point_set[point_hash].first == vertex) {
				point_idx = point_set[point_hash].second;
			} else {
				point_idx = last_solid->points.size();
				point_set[point_hash] = {vertex, point_idx};
				last_solid->points.push_back(vertex);
			}
			last_triangle->vertices[triangle_vertex_idx++] = point_idx;
		} else if (words[0] != "outer" && words[0] != "endloop") {
			throw std::runtime_error(std::string("Undefined STL keyword")); //TODO: make better exceptions
		}
	}

	return solids;
}


Mesh read_stl_binary(const std::string filename) {

	// binary STL format:
	// [80 byte header][uint32 num_triangles][per-triangle: 12 floats + uint16 attribute]
	std::ifstream input(filename, std::ios::binary);
	if (!input) {
		throw std::runtime_error("Failed to open STL file");
	}

	char header[80];
	input.read(header, sizeof(header)); // ignore header contents

	uint32_t triangle_count = 0;
	input.read(reinterpret_cast<char*>(&triangle_count), sizeof(triangle_count));
	if (!input) {
		throw std::runtime_error("Failed to read triangle count");
	}

	Mesh mesh;
	std::unordered_map<std::size_t, std::pair<vec3_t, uint32_t>> point_set;
	boost::hash<vec3_t> point_hasher;
	std::array<vec3_t, 3> current_vertices;

	for (uint32_t i = 0; i < triangle_count; ++i) {
		triangle_t tri;

		float normal[3];
		input.read(reinterpret_cast<char*>(normal), sizeof(normal));
		if (!input) throw std::runtime_error("Unexpected EOF while reading normal");
		tri.normal_vec = {normal[0], normal[1], normal[2]};

		for (int v = 0; v < 3; ++v) {
			float vertex_vals[3];
			input.read(reinterpret_cast<char*>(vertex_vals), sizeof(vertex_vals));
			if (!input) throw std::runtime_error("Unexpected EOF while reading vertex");
			vec3_t vertex{vertex_vals[0], vertex_vals[1], vertex_vals[2]};
			current_vertices[v] = vertex;

			auto point_hash = point_hasher(vertex);
			auto found = point_set.find(point_hash);
			if (found != point_set.end() && found->second.first == vertex) {
				tri.vertices[v] = found->second.second;
			} else {
				uint32_t idx = static_cast<uint32_t>(mesh.points.size());
				point_set[point_hash] = {vertex, idx};
				mesh.points.push_back(vertex);
				tri.vertices[v] = idx;
			}
		}

		// skip attribute byte count
		uint16_t attribute_byte_count = 0;
		input.read(reinterpret_cast<char*>(&attribute_byte_count), sizeof(attribute_byte_count));
		if (!input) throw std::runtime_error("Unexpected EOF while reading attribute byte count");

		// Ensure consistent winding with measured normal
		auto measured_normal = tri.compute_normal(current_vertices);
		if (tri.normal_vec.normalize() != measured_normal.normalize()) {
			std::swap(tri.vertices[1], tri.vertices[2]);
		}
		tri.compute_centroid(current_vertices);
		mesh.triangles.push_back(tri);
	}

	return mesh;
}
