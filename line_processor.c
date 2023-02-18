/*
 * --MTP--
 * Author: Gregory Navasarkian
 * To compile: gcc -std=gnu99 -pthread -o line_processor line_processor.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <err.h>
#include <errno.h>

// buffer size
#define LINE_LENGTH 1000
// number of lines
#define NUM_LINES 50
// character count of each output line
#define COUNT 80

// buffer 1, shared resource between input and line_sep thread
char buff_1[NUM_LINES][LINE_LENGTH];
// buffer 2, shared resource between line_sep and plus_sign thread
char buff_2[NUM_LINES][LINE_LENGTH];
// buffer 3, shared resource between plus_sign and output thread
char buff_3[NUM_LINES][LINE_LENGTH];

// bool to check for 'STOP' command
bool isStop = false;

// counter and index for buffer1 producer and consumer
int prod_idx_1 = 0;
int con_idx_1 = 0;
int count_1 = 0;

// counter and index for buffer2 producer and consumer
int prod_idx_2 = 0;
int con_idx_2 = 0;
int count_2 = 0;

// counter and index for buffer3 producer and consumer
int prod_idx_3 = 0;
int con_idx_3 = 0;
int count_3 = 0;

// initialize mutex's for each buffer
pthread_mutex_t mutex_1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_3 = PTHREAD_MUTEX_INITIALIZER;

// initialize condition variables for each buffer
pthread_cond_t full_1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t full_2 = PTHREAD_COND_INITIALIZER;
pthread_cond_t full_3 = PTHREAD_COND_INITIALIZER;

/*
 * Gets input from the user.
 * Returns user inputted string.
 */
char* get_user_input() {
    char *line = NULL;
    size_t lineLength = LINE_LENGTH;
    if (getline(&line, &lineLength, stdin) == 0) err(errno, "getline()");
    return line;
}

/*
 * Puts item into buff_1.
 */
void put_buff_1(char* item) {
    // lock mutex before inserting into buff_1
    pthread_mutex_lock(&mutex_1);
    // copy input into buff_1
    strcpy(buff_1[prod_idx_1], item);
    // increment index
    prod_idx_1++;
    count_1++;
    // signal to consumer that buffer is no longer empty
    pthread_cond_signal(&full_1);
    // unlock mutex
    pthread_mutex_unlock(&mutex_1);
}

/*
 * Function run by input thread.
 * Gets input from user.
 * Puts item into buff_1.
 */
void *get_input(void *args) {
    char* str;
    // continue to get user input until reach max number of inputs
    for (int i = 0; i < NUM_LINES; i++) {
        // get user input
        str = get_user_input();
        // add user input to buff_1
        put_buff_1(str);
        // check if "STOP" command has been given
        if (strcmp(str, "STOP\n") == 0) {
            isStop = true;
            break;
        }
    }
    free(str);
    return NULL;
}

/*
 * Get next string from buff_1.
 */
char* get_buff_1() {
    // Lock the mutex
    pthread_mutex_lock(&mutex_1);
    while (count_1 == 0) {
        // Buffer is empty. Wait for the producer to signal that the buffer has data
        pthread_cond_wait(&full_1, &mutex_1);
    }
    char* item = buff_1[con_idx_1];
    // Increment the index from which the item will be picked up
    con_idx_1++;
    count_1--;
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_1);
    // Return the item
    return item;
}

/*
 * Put an item into buff_2.
 */
void put_buff_2(char* item) {
    // lock mutex
    pthread_mutex_lock(&mutex_2);
    // add item to buff_2
    strcpy(buff_2[prod_idx_2], item);
    // increment index and count for buff_2
    prod_idx_2++;
    count_2++;
    // signal that buff_2 no longer empty
    pthread_cond_signal(&full_2);
    // unlock mutex
    pthread_mutex_unlock(&mutex_2);
}

/*
 * Function removes all newline chars and replaces with spaces.
 * Consume item from buff_1.
 * Produces item into buff_2 that is shared with plus_sign thread.
 */
void *line_separator(void *args) {
    char *item;
    // process input until max number of allowed inputs
    for (int i = 0; i < NUM_LINES; i++) {
        // get next item in buff_1
        item = get_buff_1();
        // loop to remove all newline characters and replace with line sep.
        for (int j = 0; j < strlen(item); j++)
            if (item[j] == '\n') item[j] = ' ';
        // add processed string to buff_2
        put_buff_2(item);
        // check if STOP command has been given
        if (strcmp(item, "STOP ") == 0 && isStop == true) break;
    }
    return NULL;
}

/*
 * Get next string from buff_2.
 */
char* get_buff_2() {
    // Lock the mutex
    pthread_mutex_lock(&mutex_2);
    while (count_2 == 0) {
        // Buffer is empty. Wait for the producer to signal that the buffer has data
        pthread_cond_wait(&full_2, &mutex_2);
    }
    char* item = buff_2[con_idx_2];
    // Increment the index from which the item will be picked up
    con_idx_2 = con_idx_2 + 1;
    count_2--;
    // Unlock the mutex
    pthread_mutex_unlock(&mutex_2);
    // Return the item
    return item;
}

/*
 * Put an item into buff_3.
 */
void put_buff_3(char* item) {
    // lock mutex
    pthread_mutex_lock(&mutex_3);
    // add item to buff_2
    strcpy(buff_3[prod_idx_3], item);
    // increment index and count for buff_2
    prod_idx_3++;
    count_3++;
    // signal that buff_2 no longer empty
    pthread_cond_signal(&full_3);
    // unlock mutex
    pthread_mutex_unlock(&mutex_3);
}

/*
 * Function replaces '++' with '^'.
 * Consume item from buff_2.
 * Produces item into buff_3 that is shared with output thread.
 */
void* plus_sign() {
    char* item;
    while(1) {
        // get next item from shared buff_2
        item = get_buff_2();
        // replace characters in string
        for (size_t i = 0; i < strlen(item); i++) {
            if (item[i] == '+' && item[i + 1] == '+') {
                char* temp = strdup(item);
                temp[i] = '%';
                temp[i + 1] = 'c';
                // replace '++' with '^'
                if (sprintf(item, temp, '^') == 0) err(errno, "sprintf()");
                free(temp);
            }
        }
        // add altered string to shared buff_3
        put_buff_3(item);
        // check if 'STOP' command has been entered
        if (strcmp(item, "STOP ") == 0 && isStop == true) break;
    }
    return NULL;
}

/*
 * Get the next item from buff_3.
 */
char* get_buff_3() {
    // lock mutex
    pthread_mutex_lock(&mutex_3);
    while (count_3 == 0) {
        // wait for producer to signal buffer is not empty
        pthread_cond_wait(&full_3, &mutex_3);
    }
    char* item = buff_3[con_idx_3];
    // increment index
    con_idx_3++;
    count_3--;
    // unlock mutex
    pthread_mutex_unlock(&mutex_3);
    return item;
}

/*
 * Function run by output_t.
 * Consume item from buff_3.
 * Print item if 80 chars or greater.
 */
void *write_output(void *args) {
    char* item;
    // initialize output string
    char output_buff[COUNT];
    // initialize output counter
    int char_count = 0;
    while (1) {
        // get next item from shared buff_3
        item = get_buff_3();
        for (size_t i = 0; i < strlen(item); i++) {
            output_buff[char_count] = item[i];
            char_count++;
            // if counter is 80 print line
            if (char_count == COUNT) {
                // reset counter
                char_count = 0;
                // print output
                fwrite(output_buff, sizeof *output_buff, sizeof(output_buff) / sizeof(*output_buff), stdout);
                putchar('\n');
                fflush(stdout);
            }
        }
        // check for 'STOP' command
        if (strcmp(item, "STOP ") == 0 && isStop == true) break;
    }
    return NULL;
}

int main(void) {
    // declare 4 threads
    pthread_t input_t, line_sep_t, plus_sign_t, output_t;
    // create threads
    pthread_create(&input_t, NULL, get_input, NULL);
    pthread_create(&line_sep_t, NULL, line_separator, NULL);
    pthread_create(&plus_sign_t, NULL, plus_sign, NULL);
    pthread_create(&output_t, NULL, write_output, NULL);
    // wait for threads to terminate
    pthread_join(input_t, NULL);
    pthread_join(line_sep_t, NULL);
    pthread_join(plus_sign_t, NULL);
    pthread_join(output_t, NULL);
    return EXIT_SUCCESS;
}
