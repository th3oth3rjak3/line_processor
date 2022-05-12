#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define LINES_TO_READ 50
#define BUFFER_SIZE 1000
#define LINE_LENGTH 80

// To hold the pointers to each character array
char *input_buffer[LINES_TO_READ];
char *space_buffer[LINES_TO_READ];
char *plus_buffer[LINES_TO_READ];
char output_buffer[BUFFER_SIZE];

int input_lines_pending = 0;
int input_lines_read = 0;
int space_lines_pending = 0;
int space_lines_read = 0;
int plus_lines_pending = 0;
int plus_lines_read = 0;

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


void collect_input_data(int index){

    char temp_buff[BUFFER_SIZE];
    memset(temp_buff, '\0', BUFFER_SIZE);
    fgets(temp_buff, BUFFER_SIZE, stdin);

    pthread_mutex_lock(&input_mutex);
    input_buffer[index] = malloc(BUFFER_SIZE * sizeof(char));
    memset(input_buffer[index], '\0', BUFFER_SIZE);
    strcpy(input_buffer[index], temp_buff);
    input_lines_pending++;
    input_lines_read++;
    pthread_cond_signal(&input_full);
    pthread_mutex_unlock(&input_mutex);

    if (strcmp(temp_buff, "STOP\n") == 0){
        read_thread_exit = 1;
    }

}

void *get_input(){

    for (int i = 0; i < LINES_TO_READ; i++){
        if (read_thread_exit == 1) break;
        collect_input_data(i);
    }
    return NULL;
}

void replace_lines(int index){

    char temp_buff[BUFFER_SIZE];
    memset(temp_buff, '\0', BUFFER_SIZE);

    pthread_mutex_lock(&input_mutex);
    while(input_lines_pending == 0){
        pthread_cond_wait(&input_full, &input_mutex);
    }

    strcpy(temp_buff, input_buffer[index]);
    if (strcmp(temp_buff, "STOP\n") == 0){
        space_thread_exit = 1;
    }
    input_lines_pending--;
    pthread_mutex_unlock(&input_mutex);

    size_t len = strlen(temp_buff);
    for (size_t i = 0; i < len; i++) {
        if (temp_buff[i] == '\n'){
            if (space_thread_exit == 0) {
                temp_buff[i] = ' ';
            }
        }
    }

    pthread_mutex_lock(&space_mutex);
    space_buffer[index] = malloc(BUFFER_SIZE * sizeof(char));
    memset(space_buffer[index], '\0', BUFFER_SIZE);
    strcpy(space_buffer[index], temp_buff);
    space_lines_pending++;
    space_lines_read++;
    pthread_cond_signal(&space_full);
    pthread_mutex_unlock(&space_mutex);
}

void *add_spaces(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (space_thread_exit == 1) break;
        replace_lines(i);
    }
    return NULL;
}

void fix_plus(int index){

    char temp_buff[BUFFER_SIZE];
    memset(temp_buff, '\0', BUFFER_SIZE);

    pthread_mutex_lock(&space_mutex);
    while(space_lines_pending == 0){
        pthread_cond_wait(&space_full, &space_mutex);
    }
    strcpy(temp_buff, space_buffer[index]);
    space_lines_pending--;
    pthread_mutex_unlock(&space_mutex);

    if (strcmp(temp_buff, "STOP\n") == 0){
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
            for (int i = write_ptr; i <= read_ptr; i++) {
                temp_buff[i] = '\0';
            }
        }
    }

    pthread_mutex_lock(&plus_mutex);
    plus_buffer[index] = malloc(BUFFER_SIZE * sizeof(char));
    memset(plus_buffer[index], '\0', BUFFER_SIZE);
    strcpy(plus_buffer[index], temp_buff);
    plus_lines_pending++;
    plus_lines_read++;
    pthread_cond_signal(&plus_full);
    pthread_mutex_unlock(&plus_mutex);

}


void *fix_plus_pair(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (plus_thread_exit == 1) break;
        fix_plus(i);
    }
    return NULL;
}

void do_output(int index){
    char temp_buff[BUFFER_SIZE];
    memset(temp_buff, '\0', BUFFER_SIZE);

    pthread_mutex_lock(&plus_mutex);
    while(plus_lines_pending == 0){
        pthread_cond_wait(&plus_full, &plus_mutex);
    }
    strcpy(temp_buff, plus_buffer[index]);

    if (strcmp(temp_buff, "STOP\n") == 0){
        output_thread_exit = 1;
    }

    if (output_thread_exit == 0) {
        plus_lines_pending--;
        pthread_mutex_unlock(&plus_mutex);

        // do output copying here.
        int char_count = 0;
        int read_ptr = strlen(output_buffer);
        size_t len = strlen(temp_buff);

        while (char_count < (int) len) {
            if (read_ptr < LINE_LENGTH) {
                output_buffer[read_ptr] = temp_buff[char_count];
                char_count++;
                read_ptr++;
            }
            if (read_ptr == LINE_LENGTH) {
                printf("%s\n", output_buffer);
                memset(output_buffer, '\0', BUFFER_SIZE);
                read_ptr = 0;
            }
        }
    }
}

void *send_output(){
    for (int i = 0; i < LINES_TO_READ; i++){
        if (output_thread_exit == 1) break;
        do_output(i);
    }
    return NULL;
}

int main() {

    //todo: thread safety implementation
    //todo: naming of functions so they don't suck
    //todo: figure out why I need 8 functions instead of 4
    //todo: comment the code

    memset(output_buffer, '\0', BUFFER_SIZE);
    pthread_t read_thread, space_thread, plus_thread, output_thread;

    pthread_create(&read_thread, NULL, get_input, NULL);
    pthread_create(&space_thread, NULL, add_spaces, NULL);
    pthread_create(&plus_thread, NULL, fix_plus_pair, NULL);
    pthread_create(&output_thread, NULL, send_output, NULL);
    pthread_join(read_thread, NULL);
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
