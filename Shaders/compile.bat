SET VULKAN_SDK=D:\sdk\VulkanSDK\1.1.73.0
SET COMPILER=%VULKAN_SDK%\Bin32/glslangValidator.exe
%COMPILER% -V Shader.vert
%COMPILER% -V Shader.frag
pause

