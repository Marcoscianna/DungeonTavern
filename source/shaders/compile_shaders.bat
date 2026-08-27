@echo off
glslc static.vert -o static.vert.spv
glslc skinning.vert -o skinning.vert.spv
glslc shadow.vert -o shadow.vert.spv
glslc shadow_anim.vert -o shadow_anim.vert.spv
glslc blinn.frag -o blinn.frag.spv
glslc shadow.frag -o shadow.frag.spv
glslc anim_blinn.frag -o anim_blinn.frag.spv
glslc emissive.frag -o emissive.frag.spv
pause