#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define LINES_TO_READ 50
#define BUFFER_SIZE 1000
#define LINE_LENGTH 80

// To hold the pointers to each character array
char *input_buffer[LINES_TO_READ];
char *space_buffer[LINES_TO_READ];
char *plus_buffer[LINES_TO_READ];
char output_buffer[BUFFER_SIZE];

int input_lines_pending = 0;
int space_lines_pending = 0;
int plus_lines_pending = 0;

pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t space_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t plus_mutex = PTHREAD_MUTEX_INITIALIZER;

// Condition variable to let 2nd thread know there's data
pthread_cond_t input_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t space_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t plus_full = PTHREAD_COND_INITIALIZER;

int read_thread_exit = 0;
int space_thread_exit = 0;
int plus_thread_exit = 0;
int output_thread_exit = 0;

void *collect_input(){

    for (int i = 0; i < LINES_TO_READ; i++){
        if (read_thread_exit == 1) break;

        char temp_buff[BUFFER_SIZE];
        memset(temp_buff, '\0', BUFFER_SIZE);
        fgets(temp_buff, BUFFER_SIZE, stdin);

        pthread_mutex_lock(&input_mutex);
        input_buffer[i] = malloc(BUFFER_SIZE * sizeof(char));
        memset(input_buffer[i], '\0', BUFFER_SIZE);
        strcpy(input_buffer[i], temp_buff);
        input_lines_pending++;
        pthread_cond_signal(&input_full);
        pthread_mutex_unlock(&input_mutex);

        if (strcmp(temp_buff, "STOP\n") == 0 || strcmp(temp_buff, "\n") == 0){
            read_thread_exit = 1;
        }
    }
    return NULL;
}

void *add_spaces(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (space_thread_exit == 1) break;

        char temp_buff[BUFFER_SIZE];
        memset(temp_buff, '\0', BUFFER_SIZE);

        pthread_mutex_lock(&input_mutex);
        while(input_lines_pending == 0){
            pthread_cond_wait(&input_full, &input_mutex);
        }

        strcpy(temp_buff, input_buffer[i]);
        if (strcmp(temp_buff, "STOP\n") == 0 || strcmp(temp_buff, "\n") == 0){
            space_thread_exit = 1;
        }

        input_lines_pending--;
        pthread_mutex_unlock(&input_mutex);

        size_t len = strlen(temp_buff);
        for (size_t j = 0; j < len; j++) {
            if (temp_buff[j] == '\n'){
                if (space_thread_exit == 0) {
                    temp_buff[j] = ' ';
                }
            }
        }

        pthread_mutex_lock(&space_mutex);
        space_buffer[i] = malloc(BUFFER_SIZE * sizeof(char));
        memset(space_buffer[i], '\0', BUFFER_SIZE);
        strcpy(space_buffer[i], temp_buff);
        space_lines_pending++;
        pthread_cond_signal(&space_full);
        pthread_mutex_unlock(&space_mutex);

    }
    return NULL;
}

void *fix_plus_pair(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (plus_thread_exit == 1) break;

        char temp_buff[BUFFER_SIZE];
        memset(temp_buff, '\0', BUFFER_SIZE);

        pthread_mutex_lock(&space_mutex);
        while(space_lines_pending == 0){
            pthread_cond_wait(&space_full, &space_mutex);
        }
        strcpy(temp_buff, space_buffer[i]);
        space_lines_pending--;
        pthread_mutex_unlock(&space_mutex);

        if (strcmp(temp_buff, "STOP\n") == 0 || strcmp(temp_buff, "\n") == 0){
            plus_thread_exit = 1;
        }

        if (plus_thread_exit == 0) {
            size_t len = strlen(temp_buff);
            int read_ptr = 0;
            int write_ptr = 0;
            while (read_ptr < (int)len) {
                if (temp_buff[read_ptr] == '+' && read_ptr < BUFFER_SIZE - 1 && temp_buff[read_ptr + 1] == '+') {
                    temp_buff[write_ptr] = '^';
                    read_ptr++;
                } else {
                    temp_buff[write_ptr] = temp_buff[read_ptr];
                }
                read_ptr++;
                write_ptr++;
            }

            if (read_ptr != write_ptr) {
                for (int j = write_ptr; j <= read_ptr; j++) {
                    temp_buff[j] = '\0';
                }
            }
        }

        pthread_mutex_lock(&plus_mutex);
        plus_buffer[i] = malloc(BUFFER_SIZE * sizeof(char));
        memset(plus_buffer[i], '\0', BUFFER_SIZE);
        strcpy(plus_buffer[i], temp_buff);
        plus_lines_pending++;
        pthread_cond_signal(&plus_full);
        pthread_mutex_unlock(&plus_mutex);

    }
    return NULL;
}

void *send_output(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (output_thread_exit == 1) break;

        char temp_buff[BUFFER_SIZE];
        memset(temp_buff, '\0', BUFFER_SIZE);

        pthread_mutex_lock(&plus_mutex);
        while(plus_lines_pending == 0){
            pthread_cond_wait(&plus_full, &plus_mutex);
        }
        strcpy(temp_buff, plus_buffer[i]);

        if (strcmp(temp_buff, "STOP\n") == 0 || strcmp(temp_buff, "\n") == 0){
            output_thread_exit = 1;
        }

        if (output_thread_exit == 0) {
            plus_lines_pending--;
            pthread_mutex_unlock(&plus_mutex);

            // do output copying here.
            int char_count = 0;
            size_t read_ptr = strlen(output_buffer);
            size_t len = strlen(temp_buff);

            while (char_count < (int) len) {
                if (read_ptr < LINE_LENGTH) {
                    output_buffer[read_ptr] = temp_buff[char_count];
                    char_count++;
                    read_ptr++;
                }
                if (read_ptr == LINE_LENGTH) {
                    output_buffer[LINE_LENGTH] = '\n';
                    write(STDOUT_FILENO, output_buffer, LINE_LENGTH + 1);
                    memset(output_buffer, '\0', BUFFER_SIZE);
                    read_ptr = 0;
                }
            }
        }
    }
    return NULL;
}

int main() {
    //todo: comment the code

    memset(output_buffer, '\0', BUFFER_SIZE);
    pthread_t input_thread, space_thread, plus_thread, output_thread;

    pthread_create(&input_thread, NULL, collect_input, NULL);
    pthread_create(&space_thread, NULL, add_spaces, NULL);
    pthread_create(&plus_thread, NULL, fix_plus_pair, NULL);
    pthread_create(&output_thread, NULL, send_output, NULL);
    pthread_join(input_thread, NULL);
    pthread_join(space_thread, NULL);
    pthread_join(plus_thread, NULL);
    pthread_join(output_thread, NULL);


    for (int i = 0; i < LINES_TO_READ; i++){
        free(input_buffer[i]);
        free(space_buffer[i]);
        free(plus_buffer[i]);
    }

    return EXIT_SUCCESS;
}
