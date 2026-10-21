CC=gcc
CFLAGS = -Wall -Wextra -O2 -lm -lpthread -Iinclude 
SRC_DIR = src
SRC = $(wildcard $(SRC_DIR)/*.c)
TARGET = mc

# Main rule 
$(TARGET): $(SRC) 
	$(CC) $(SRC) $(CFLAGS) -o $(TARGET)

# clean and recompile 
all: clean $(TARGET)  

# execute file 
run: 
	./$(TARGET) 

# clean generated files
clean:
	rm -rf $(TARGET) 
