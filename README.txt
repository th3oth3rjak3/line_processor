Name: Jake Hathaway
Date: 5/11/2022
Description:
    line_processor is a multi-threaded pipeline which uses 4 threads to receive input, replaces any newline characters
with spaces, converts any occurrence of "++" into a "^", and then prints out lines of exactly 80 characters. If the
phrase "STOP" is encountered on its own line, no additional lines of text will be processed and the program will exit.

Compiling:
    To compile and run this program, use the terminal command below:

    gcc -std=c99 -o line_processor line_processor.c -pthread -Wextra -Wpedantic -Wall -Werror

To run:
    The program may be run by typing input into the terminal or through file input redirection. Results of the data
processing may be output to another file as well.

Interactive:                    ./line_processor
Input redirection:              ./line_processor < [YOUR FILENAME]
Output redirection:             ./line_processor > [YOUR FILENAME]
Input and Output Redirection:   ./line_processor < [INPUT FILENAME] > [OUTPUT FILENAME]
