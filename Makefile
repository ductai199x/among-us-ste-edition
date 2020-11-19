CXX := g++-9
SERVER_EXEC ?= server.o
CLIENT_EXEC ?= client.o
EXAMPLE_PROGRAM ?= program.o
DUNGEON ?= dungeon.o

HELPER_SOURCE := helper/GameInstance.cpp

INC_DIRS := ./asio-1.18.0
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS)

SEVER_CXXFLAGS ?= -lpthread -lstdc++fs -std=c++17
CLIENT_CXXFLAGS ?= -lX11 -lGL -lpthread -lpng -lstdc++fs -std=c++17

.PHONY: clean client server all

default: server client

client: Client.cpp helper/GameInstance.cpp
	$(CXX) $(CPPFLAGS) $? $(CLIENT_CXXFLAGS) -o $(CLIENT_EXEC)

server: Server.cpp helper/GameInstance.cpp
	$(CXX) $(CPPFLAGS) $? $(SEVER_CXXFLAGS) -o $(SERVER_EXEC)

program: MapDesigner.cpp
	$(CXX) -I./helper $? $(CLIENT_CXXFLAGS) -o $(EXAMPLE_PROGRAM)

clean:
	$(RM) -rf *.o

-include $(DEPS)

MKDIR_P ?= mkdir -p
