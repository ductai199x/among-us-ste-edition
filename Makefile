CXX := g++
SERVER_EXEC ?= server.o
CLIENT_EXEC ?= client.o
EXAMPLE_PROGRAM ?= program.o

BUILD_DIR ?= ./build
# SRC_DIRS ?= 

# SRCS := $(shell find $(SRC_DIRS) -name '*.h' -or -name '*.hpp' -or -name '*.cpp')
# OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
# DEPS := $(OBJS:.o=.d)

INC_DIRS := ./networking ./pgex
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS)

CXXFLAGS ?= -lX11 -lGL -lpthread -lpng -lstdc++fs -std=c++17

.PHONY: clean client server all

client: SimpleClient.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $< $(CXXFLAGS) -o $(CLIENT_EXEC)

server: SimpleServer.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $< $(CXXFLAGS) -o $(SERVER_EXEC)

program: olcExampleProgram.cpp
	echo $(INC_DIRS)
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $< $(CXXFLAGS) -o $(EXAMPLE_PROGRAM)



clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
