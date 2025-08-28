# Compiler to use
CC = gcc

# Compiler flags:
# -Wall: Enable all compiler's warning messages
# -g:    Add debugging information to the executable
# -std=c99: Use the C99 standard
CFLAGS = -Wall -g -std=c99 -D_GNU_SOURCE

# The name of the final executable
TARGET = kfuzztest_bridge

# List of all source files (.c)
SRCS = kfuzztest_bridge.c kfuzztest_input_lexer.c kfuzztest_input_parser.c kfuzztest_encoder.c

# Automatic list of object files (.o) based on the source files
OBJS = $(SRCS:.c=.o)

# The default rule, which is executed when you just run `make`
# This rule depends on the executable target.
all: $(TARGET)

# Rule to link all object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Generic rule to compile a .c source file into a .o object file
# The '-c' flag tells the compiler to compile but not link.
# '$<' is an automatic variable that holds the name of the first prerequisite (the .c file).
# '$@' is an automatic variable that holds the name of the target (the .o file).
%.o: %.c kfuzztest_input_lexer.h
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to run the compiled program
run: $(TARGET)
	./$(TARGET)

# Rule to clean up the directory by removing generated files
clean:
	rm -f $(OBJS) $(TARGET)

# Declaring targets that are not actual files
.PHONY: all clean run
