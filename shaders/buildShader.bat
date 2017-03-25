del *.spv
glslangValidator.exe -V shader.vert -o vert.spv
glslangValidator.exe -V shader.frag -o frag.spv

glslangValidator.exe -V widgetShader.vert -o widgetVert.spv
glslangValidator.exe -V widgetShader.frag -o widgetFrag.spv

glslangValidator.exe -V skybox.vert -o skyboxVert.spv
glslangValidator.exe -V skybox.frag -o skyboxFrag.spv


pause