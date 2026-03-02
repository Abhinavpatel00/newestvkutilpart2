# =========================================
# Config
# =========================================

APP_DEBUG   := build/app_debug

CC  := clang
CXX := clang++
TARGET :=  build/app
BUILD_DIR := build

# List your C and C++ source files
SRC_C   := main.c ext.c vk.c helpers.c cachestuff.c offset_allocator.c
SRC_CPP := vma.cpp  $(wildcard external/meshoptimizer/src/*.cpp) \
		   external/cimgui/cimgui.cpp external/cimgui/cimgui_impl.cpp \
		   external/cimgui/imgui/imgui.cpp external/cimgui/imgui/imgui_draw.cpp \
		   external/cimgui/imgui/imgui_demo.cpp external/cimgui/imgui/imgui_tables.cpp \
		   external/cimgui/imgui/imgui_widgets.cpp \
		   external/cimgui/imgui/backends/imgui_impl_glfw.cpp \
		   external/cimgui/imgui/backends/imgui_impl_vulkan.cpp

# Compiler flags
CFLAGS   := -std=gnu99 -ggdb  


CXXFLAGS := -std=c++17 -w -g -fno-common -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES \
		   -DIMGUI_IMPL_API='extern "C"' \
		   -Iexternal/cimgui -Iexternal/cimgui/imgui -Iexternal/cimgui/imgui/backends
LDFLAGS  :=
LIBS := -lvulkan -lm -lglfw -lX11 -lXi -lXrandr -lXcursor -lXinerama -ldl -lpthread
# 1. Transform source names to build folder object names
# This turns 'test.c' into 'build/test.o'
OBJ := $(addprefix $(BUILD_DIR)/, $(SRC_C:.c=.o) $(SRC_CPP:.cpp=.o))

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo Linking $@
	$(CXX) $^ -o $@ $(LIBS)

# C compilation
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo Compiling $<
	$(CC) $(CFLAGS) -c $< -o $@

# C++ compilation
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo Compiling $<
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	@echo Cleaning...
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean
.PHONY: all clean
