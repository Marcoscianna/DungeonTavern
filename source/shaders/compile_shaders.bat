@echo off
glslc static.vert -o static.vert.spv
glslc skinning.vert -o skinning.vert.spv
glslc blinn.frag -o blinn.frag.spv
glslc emissive.frag -o emissive.frag.spv
pause