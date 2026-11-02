CXX = clang++
CXXFLAGS = -std=c++20 -O3 -Wall -Wextra -pthread -Iinclude
LDFLAGS = -pthread

SRCS = src/pipe_channel.cpp \
       src/shm_channel.cpp \
       src/mq_channel.cpp \
       src/socket_channel.cpp \
       src/benchmark_suite.cpp \
       src/main.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = omni_ipc_engine

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f /tmp/omni_*

.PHONY: all clean
