#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define LINES_TO_READ 50
#define BUFFER_SIZE 1000
#define LINE_LENGTH 80

// Shared buffers. To be used inside a mutex lock only.
char *input_buffer[LINES_TO_READ];
char *space_buffer[LINES_TO_READ];
char *plus_buffer[LINES_TO_READ];

// Global buffer only used by output thread.
char output_buffer[BUFFER_SIZE];

// Used to indicate state of each buffer.
int input_lines_pending = 0;
int space_lines_pending = 0;
int plus_lines_pending = 0;

// Mutex locks for each of the shared sections, meant to protect the associated buffers.
pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t space_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t plus_mutex = PTHREAD_MUTEX_INITIALIZER;

// Condition variable to check for mutex lock when waiting.
pthread_cond_t input_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t space_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t plus_full = PTHREAD_COND_INITIALIZER;

// Global variables to indicate if a thread should exit.
int read_thread_exit = 0;
int space_thread_exit = 0;
int plus_thread_exit = 0;
int output_thread_exit = 0;

/************************************************************
 *                      collect_input
 * This function is used to get user input from STDIN and
 * place it into the first shared buffer by the first thread.
 * No data processing occurs in this step.
************************************************************/

void *collect_input(){

    for (int i = 0; i < LINES_TO_READ; i++){
        if (read_thread_exit == 1) break;                       // Used to exit the thread naturally via pthread_join()

        char temp_buff[BUFFER_SIZE];                            // Used to reduce time inside the mutex lock
        memset(temp_buff, '\0', BUFFER_SIZE);
        fgets(temp_buff, BUFFER_SIZE, stdin);                   // Copy input to temporary buffer
        pthread_mutex_lock(&input_mutex);
        input_buffer[i] = malloc(BUFFER_SIZE * sizeof(char));
        memset(input_buffer[i], '\0', BUFFER_SIZE);
        strcpy(input_buffer[i], temp_buff);                     // Copy input to shared buffer
        input_lines_pending++;                                  // Indicate new line is ready for processing
        pthread_cond_signal(&input_full);                       // Signal condition is met to next thread
        pthread_mutex_unlock(&input_mutex);                     // Release the mutex lock

        // Check for STOP on its own line or an empty line, set exit condition if either are true.
        if (strcmp(temp_buff, "STOP\n") == 0 || strcmp(temp_buff, "\n") == 0){
            read_thread_exit = 1;
        }
    }
    return NULL;
}

/************************************************************
 *                        add_spaces
 * This function is used to convert any newline characters
 * into spaces. The thread that uses this function takes
 * input from the first shared buffer, replaces newlines
 * with spaces, and puts the results into the second shared
 * buffer.
************************************************************/
void *add_spaces(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (space_thread_exit == 1) break;                      // Used to exit the thread naturally via pthread_join()

        char temp_buff[BUFFER_SIZE];
        memset(temp_buff, '\0', BUFFER_SIZE);

        pthread_mutex_lock(&input_mutex);                       // Acquire Input Mutex lock
        while(input_lines_pending == 0){                        // Wait for signal if no lines are pending.
            pthread_cond_wait(&input_full, &input_mutex);
        }

        strcpy(temp_buff, input_buffer[i]);
        if (strcmp(temp_buff, "STOP\n") == 0 || strcmp(temp_buff, "\n") == 0){
            space_thread_exit = 1;                              // Check for exit conditions
        }

        input_lines_pending--;                                  // Decrement line indicator after processing
        pthread_mutex_unlock(&input_mutex);                     // Release mutex lock

        size_t len = strlen(temp_buff);
        for (size_t j = 0; j < len; j++) {
            if (temp_buff[j] == '\n'){                          // Loop through input and look for newline characters
                if (space_thread_exit == 0) {
                    temp_buff[j] = ' ';                         // Replace newline with space
                }
            }
        }

        pthread_mutex_lock(&space_mutex);                       // Acquire mutex lock for second shared buffer
        space_buffer[i] = malloc(BUFFER_SIZE * sizeof(char));   // Allocate memory to hold data
        memset(space_buffer[i], '\0', BUFFER_SIZE);
        strcpy(space_buffer[i], temp_buff);                     // Copy processed data into shared buffer
        space_lines_pending++;                                  // Indicate pending lines to process for next thread
        pthread_cond_signal(&space_full);                       // Signal next thread
        pthread_mutex_unlock(&space_mutex);                     // Release mutex lock

    }
    return NULL;
}

/************************************************************
 *                      fix_plus_pair
 * This function is used collect input from the second shared
 * buffer by the third thread. It replaces any occurrences of
 * ++ with ^ and then stores the results in the third shared
 * buffer.
************************************************************/

void *fix_plus_pair(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (plus_thread_exit == 1) break;                           // Exit naturally via pthread_join

        char temp_buff[BUFFER_SIZE];
        memset(temp_buff, '\0', BUFFER_SIZE);

        pthread_mutex_lock(&space_mutex);                           // Acquire mutex lock for second shared buffer
        while(space_lines_pending == 0){
            pthread_cond_wait(&space_full, &space_mutex);           // Wait for signal if no lines pending
        }
        strcpy(temp_buff, space_buffer[i]);                         // Copy to temp_buff to reduce time in mutex lock
        space_lines_pending--;                                      // Reduce lines pending by 1
        pthread_mutex_unlock(&space_mutex);                         // Unlock the mutex

        if (strcmp(temp_buff, "STOP\n") == 0 || strcmp(temp_buff, "\n") == 0){
            plus_thread_exit = 1;                                   // Check for exit conditions and set flag if true
        }

        if (plus_thread_exit == 0) {                                // If no exit condition set, process lines
            size_t len = strlen(temp_buff);
            int read_ptr = 0;                                       // Used to get the next char to be processed
            int write_ptr = 0;                                      // Used to keep track of where to write next char
            while (read_ptr < (int)len) {
                // Checking to see if "++" exists, if so replace with "^"
                if (temp_buff[read_ptr] == '+' && read_ptr < BUFFER_SIZE - 1 && temp_buff[read_ptr + 1] == '+') {
                    temp_buff[write_ptr] = '^';
                    read_ptr++;                                     // Increment the read pointer to the second "+"
                } else {
                    temp_buff[write_ptr] = temp_buff[read_ptr];     // Copy character from read_ptr to write_ptr
                }
                read_ptr++;
                write_ptr++;
            }

            // If replacement has occurred, writing will finish before the read_ptr position, leaving garbage characters.
            if (read_ptr != write_ptr) {
                for (int j = write_ptr; j <= read_ptr; j++) {
                    temp_buff[j] = '\0';                            // Replace garbage characters with '\0'
                }
            }
        }

        pthread_mutex_lock(&plus_mutex);                            // Lock the mutex for the third shared buffer
        plus_buffer[i] = malloc(BUFFER_SIZE * sizeof(char));        // Allocate space...
        memset(plus_buffer[i], '\0', BUFFER_SIZE);
        strcpy(plus_buffer[i], temp_buff);                          // Copy to final shared buffer
        plus_lines_pending++;                                       // Indicate a line has been processed
        pthread_cond_signal(&plus_full);                            // Signal to next thread
        pthread_mutex_unlock(&plus_mutex);                          // Unlock mutex

    }
    return NULL;
}
/************************************************************
 *                      send_output
 * This function is used to get data from the third shared
 * buffer by the fourth thread. It collects input into its
 * own output buffer until LINE_LENGTH characters have been
 * stored. Once the output buffer contains enough characters
 * the contents of the buffer will be written to STDOUT.
 * The buffer will then be cleared and the function will
 * continue processing until the end of the input line.
************************************************************/

void *send_output(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (output_thread_exit == 1) break;                         // Exit naturally via pthread_join

        char temp_buff[BUFFER_SIZE];
        memset(temp_buff, '\0', BUFFER_SIZE);

        pthread_mutex_lock(&plus_mutex);                            // Acquire mutex lock
        while(plus_lines_pending == 0){
            pthread_cond_wait(&plus_full, &plus_mutex);             // Wait for pending lines
        }
        strcpy(temp_buff, plus_buffer[i]);

        if (strcmp(temp_buff, "STOP\n") == 0 || strcmp(temp_buff, "\n") == 0){
            output_thread_exit = 1;                                 // Check for exit conditions and set flag
        }
        plus_lines_pending--;                                       // Decrement lines pending
        pthread_mutex_unlock(&plus_mutex);                          // Unlock third buffer mutex

        if (output_thread_exit == 0) {                              // Process data only if not flagged to exit
            int char_count = 0;                                     // Used as exit condition for while loop
            size_t read_ptr = strlen(output_buffer);                // Number of characters already in output buffer
            size_t len = strlen(temp_buff);                         // Length of the string in the buffer

            while (char_count < (int) len) {                        // Once we get to the end of the string, exit the loop
                if (read_ptr < LINE_LENGTH) {
                    output_buffer[read_ptr] = temp_buff[char_count];    // Put the new character in the output buffer
                    char_count++;                                       // Increment the number of characters read
                    read_ptr++;                                         // Increment the position in the output buffer
                }
                if (read_ptr == LINE_LENGTH) {                          // If the output buffer is ready to print
                    output_buffer[LINE_LENGTH] = '\n';                  // Add the newline character to the end
                    write(STDOUT_FILENO, output_buffer, LINE_LENGTH + 1);   // Write the line plus the additional newline
                    memset(output_buffer, '\0', BUFFER_SIZE);               // Clear out the buffer
                    read_ptr = 0;                                       // Reset the position to 0
                }
            }
        }
    }
    return NULL;
}

/************************************************************
 *                          main
 * This function creates 4 threads and assigns them work.
 * It waits for the threads to finish via pthread_join. After
 * work is complete, all the allocated memory is deallocated
 * via the free command.
************************************************************/

int main() {

    memset(output_buffer, '\0', BUFFER_SIZE);                           // Initialize output_buffer to null chars
    pthread_t input_thread, space_thread, plus_thread, output_thread;   // Declare threads

    pthread_create(&input_thread, NULL, collect_input, NULL);           // Create the input thread
    pthread_create(&space_thread, NULL, add_spaces, NULL);              // Create the thread that replaces newlines with spaces
    pthread_create(&plus_thread, NULL, fix_plus_pair, NULL);            // Create the thread that replaces ++ with ^
    pthread_create(&output_thread, NULL, send_output, NULL);            // Create the thread that writes output
    pthread_join(input_thread, NULL);
    pthread_join(space_thread, NULL);
    pthread_join(plus_thread, NULL);
    pthread_join(output_thread, NULL);

    // Free all mallocs so no memory leaks occur
    for (int i = 0; i < LINES_TO_READ; i++){
        free(input_buffer[i]);
        free(space_buffer[i]);
        free(plus_buffer[i]);
    }

    return EXIT_SUCCESS;
}
