
COMPILER = clang++

SOURCES = main.cpp bootstrap/VkBootstrap.cpp

FLAGS = -Wall -ferror-limit=100

VULKAN_INCLUDE_PATH = /usr/local/include
GLFW_INCLUDE_PATH = /usr/local/Cellar/glfw/3.4/include/GLFW
SHADERC_INCLUDE_PATH = /usr/local/Cellar/shaderc/2024.0/include
INCLUDE = -I$(VULKAN_INCLUDE_PATH) -I$(GLFW_INCLUDE_PATH) -I$(SHADERC_INCLUDE_PATH) -I/Users/macfarrell/programming_projects/partilce/bootstrap

LIBVULKAN_PATH = /Users/macfarrell/VulkanSDK/1.3.261.1/macOS/lib
LIBGLFW_PATH = /usr/local/Cellar/glfw/3.4/lib
LIBSHADERC_PATH = /usr/local/Cellar/shaderc/2024.0/lib
LINK = -L$(LIBVULKAN_PATH) -L$(LIBGLFW_PATH) -L$(LIBSHADERC_PATH) -lvulkan -lglfw -lshaderc_shared

debug: $(ENTRY_POINT)
	$(COMPILER) $(SOURCES) -o main $(FLAGS) $(INCLUDE) $(LINK)

