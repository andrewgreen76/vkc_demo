CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -pedantic -O2
LDFLAGS := -lglfw -lvulkan
TARGET := vulkan_triangle
SRC := src/main.c
SHADERS := shaders/triangle.vert.spv shaders/triangle.frag.spv

all: $(TARGET)

$(TARGET): $(SRC) $(SHADERS)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

shaders/triangle.vert.spv: shaders/triangle.vert
	glslc shaders/triangle.vert -o $@

shaders/triangle.frag.spv: shaders/triangle.frag
	glslc shaders/triangle.frag -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(SHADERS)

.PHONY: all run clean
