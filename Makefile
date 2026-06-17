CC = g++
CFLAGS = -Iinclude -Wall -std=c++17
SRC = src/myShell.cpp src/parser.cpp src/builtins.cpp src/command.cpp src/executor.cpp src/jobs.cpp src/config.cpp src/history.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = tinyshell

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
