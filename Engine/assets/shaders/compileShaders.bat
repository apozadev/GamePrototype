fxc -E main -T ps_5_0 -Fo errorP.cso errorP.hlsl
fxc -E main -T vs_5_0 -Fo errorV.cso errorV.hlsl

glslc -fshader-stage=fragment errorP.glsl -o errorP.spv
glslc -fshader-stage=vertex errorV.glsl -o errorV.spv

pause