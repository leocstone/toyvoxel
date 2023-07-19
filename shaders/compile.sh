#!/bin/bash
glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv
glslc shader.comp -o compute.spv
glslc shader_distances.comp -o compute_distances.spv
