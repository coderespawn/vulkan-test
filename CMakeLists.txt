cmake_minimum_required (VERSION 2.6)
project (VulkanTutorial)

SET (DIR_GLFW 	"D:/sdk/VulkanLibraries/glfw-3.2.1.bin.WIN64" CACHE FILEPATH "GLFW Library Path")
SET (DIR_GLM	"D:/sdk/VulkanLibraries/glm-0.9.9-a2" CACHE FILEPATH "GLM Library Path")
SET (DIR_STB	"D:/sdk/VulkanLibraries/stb" CACHE FILEPATH "STB Library Path")
SET (DIR_VULKAN	"D:/sdk/VulkanSDK/1.1.73.0" CACHE FILEPATH "Vulkan SDK Path")
SET (CMAKE_INSTALL_PREFIX "../Install")

add_executable(VulkanTutorial
	Source/main.cpp
)

target_link_libraries(VulkanTutorial 
	${DIR_VULKAN}/Lib/vulkan-1.lib
	${DIR_GLFW}/lib-vc2015/glfw3.lib
)

IF (MSVC)
	SET_TARGET_PROPERTIES(VulkanTutorial PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB:msvcrt.lib")
ENDIF()

INCLUDE_DIRECTORIES(
	${DIR_VULKAN}/include
	${DIR_GLFW}/include
	${DIR_GLM}
	${DIR_STB}
)
