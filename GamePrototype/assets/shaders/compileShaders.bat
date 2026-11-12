fxc -E main -T ps_5_0 -Fo triangleP.cso -Od -Zi triangleP.hlsl
fxc -E main -T vs_5_0 -Fo triangleV.cso -Od -Zi triangleV.hlsl

glslc -g -fshader-stage=fragment triangleP.glsl -o triangleP.spv
glslc -g -fshader-stage=vertex triangleV.glsl -o triangleV.spv

pause