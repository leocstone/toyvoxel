#!/bin/bash
glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv
glslc shader.comp -o compute.spv
