/* The code that test the sorting algorithms in kernel space
 */
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#define SORT_DEV "/dev/sort_test"

#define MAX_LEN ((1 << 14) + 10)
#define MIN_LEN 4
#define LOOP 100

#define LISTSORT "ls_data"
#define TIMMERGE "tm_data"
#define TIMLINEAR "tl_data"
#define TIMBINARY "tb_data"
#define TIMLGALLOP "tlg_data"
#define TIMBGALLOP "tbg_data"
#define ADAPSHIVER "ads_data"
#define ADSMERGE "adsm_data"

unsigned long long int duration[LOOP];
unsigned long long int count[LOOP];
double k[LOOP];

typedef struct {
    char *name;
} directory;

directory dirs[] = {
    {.name = LISTSORT}, 
    {.name = TIMMERGE}, 
    {.name = TIMLINEAR}, 
    {.name = TIMBINARY}, 
    {.name = TIMLGALLOP}, 
    {.name = TIMBGALLOP}, 
    {.name = ADAPSHIVER}, 
    {.name = ADSMERGE},
    {.name = NULL}
};

static void file_output(size_t num, int case_id, char *dir_name)
{
    char cnt_file[100];
    char time_file[100];
    char k_file[100];

    switch (case_id) {
    case 0: /* Worst case of merge sort */
        sprintf(cnt_file, "%s/w_count.txt", dir_name);
        sprintf(time_file, "%s/w_time.txt", dir_name);
        sprintf(k_file, "%s/w_kvalue.txt", dir_name);
        break;
    case 1: /* Random 3 elements */
        sprintf(cnt_file, "%s/r3_count.txt", dir_name);
        sprintf(time_file, "%s/r3_time.txt", dir_name);
        sprintf(k_file, "%s/r3_kvalue.txt", dir_name);
        break;
    case 2: /* Random last 10 elements */
        sprintf(cnt_file, "%s/rl10_count.txt", dir_name);
        sprintf(time_file, "%s/rl10_time.txt", dir_name);
        sprintf(k_file, "%s/rl10_kvalue.txt", dir_name);
        break;
    case 3: /* Random 1% elements */
        sprintf(cnt_file, "%s/r1p_count.txt", dir_name);
        sprintf(time_file, "%s/r1p_time.txt", dir_name);
        sprintf(k_file, "%s/r1p_kvalue.txt", dir_name);
        break;
    case 4: /* Duplicate */
        sprintf(cnt_file, "%s/dup_count.txt", dir_name);
        sprintf(time_file, "%s/dup_time.txt", dir_name);
        sprintf(k_file, "%s/dup_kvalue.txt", dir_name);
        break;
    default: /* Random elements */
        sprintf(cnt_file, "%s/r_count.txt", dir_name);
        sprintf(time_file, "%s/r_time.txt", dir_name);
        sprintf(k_file, "%s/r_kvalue.txt", dir_name);
        break;
    }

    FILE *cnt = fopen(cnt_file, "a");
    FILE *time = fopen(time_file, "a");
    FILE *kvalue = fopen(k_file, "a");

    if (!cnt) {
        perror("The output file `(.)_count.txt` might have been collapsed");
        exit(EXIT_FAILURE);
    }

    if (!time) {
        perror("The output file `(.)_time.txt` might have been collapsed");
        exit(EXIT_FAILURE);
    }

    if (!kvalue) {
        perror("The output file `(.)_kvalue.txt` might have been collapsed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0 ; i < LOOP ; i++) {
        fprintf(cnt, "%lu %llu\n", num, count[i]);
        fprintf(time, "%lu %llu\n", num, duration[i]);
        fprintf(kvalue, "%lu %f\n", num, k[i]);
    }

    fclose(cnt);
    fclose(time);
    fclose(kvalue);
}

/* To get the k-value from the current number of comparisons and nodes */
double k_value(size_t n, size_t comp)
{
    return log2(n) - (double) (comp - 1) / n;
}

static void sort_test_one_num(int num)
{
    if (num < MIN_LEN || num > MAX_LEN) {
        perror("Given number argument out of the range of number of nodes in this code");
        exit(EXIT_FAILURE);
    }
    
    int sort_id = 0;
    for (int case_id = 0 ; case_id < 6 ; case_id++) {
        for (directory *dir = dirs ; dir->name ; sort_id++, dir++) {
            for (int i = 0 ; i < LOOP ; i++) {
                int fd = open(SORT_DEV, O_RDWR);
                if (fd < 0) {
                    perror("Failed to open character device");
                    exit(EXIT_FAILURE);
                }

                char buf_write[512];
                sprintf(buf_write, "%d %d %d", num, case_id, sort_id);
                /* write the properties of the current test case to device driver */
                ssize_t w_sz = write(fd, buf_write, 512);
                if (w_sz < 0) {
                    perror("Failed to write to the device");
                    close(fd);
                    exit(EXIT_FAILURE);
                }

                char buf_read[512];
                /* read the output of the test from device driver */
                ssize_t r_sz = read(fd, &buf_read, 512);
                if (r_sz < 0) {
                    perror("Failed to read from the device");
                    close(fd);
                    exit(EXIT_FAILURE);
                }

                /* handle the string read from the kernel device */
                char *token, *endptr; 
                int counter = 0; /* the printing state of the tokens */
                token = strtok(buf_read, " \t\r\n\a");
                while(token != NULL){
                    if(counter > 2) 
                        break;

                    unsigned long long int num = strtoull(token, &endptr, 10);
                    if (counter == 0)
                        duration[i] = num;
                    else if(counter == 1)
                        count[i] = num;
                    
                    token = strtok(NULL, " \t\r\n\a");
                    counter++;
                }

                k[i] = k_value((size_t) num, (size_t) count[i]);
                close(fd);
            }
            file_output(num, case_id, dir->name);
        }
        sort_id = 0;
    }
}

static void sort_test_continuously()
{
    int sort_id = 0;
    for (int case_id = 0 ; case_id < 6 ; case_id++) {
        for (directory *dir = dirs ; dir->name ; sort_id++, dir++) {
            for (int num = MIN_LEN ; num < MAX_LEN ; num++) {
                // printf("start %s in #%d\n", dir->name, num);
                for (int i = 0 ; i < LOOP ; i++) {
                    int fd = open(SORT_DEV, O_RDWR);
                    if (fd < 0) {
                        perror("Failed to open character device");
                        exit(EXIT_FAILURE);
                    }

                    char buf_write[512];
                    sprintf(buf_write, "%d %d %d", num, case_id, sort_id);
                    /* write the properties of the current test case to device driver */
                    ssize_t w_sz = write(fd, buf_write, 512);
                    if (w_sz < 0) {
                        perror("Failed to write to the device");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    char buf_read[512];
                    /* read the output of the test from device driver */
                    ssize_t r_sz = read(fd, &buf_read, 512);
                    if (r_sz < 0) {
                        perror("Failed to read from the device");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    /* handle the string read from the kernel device */
                    char *token, *endptr; 
                    int counter = 0; /* the printing state of the tokens */
                    token = strtok(buf_read, " \t\r\n\a");
                    while(token != NULL){
                        if(counter > 2) 
                            break;

                        unsigned long long int num = strtoull(token, &endptr, 10);
                        if (counter == 0)
                            duration[i] = num;
                        else if(counter == 1)
                            count[i] = num;
                        
                        token = strtok(NULL, " \t\r\n\a");
                        counter++;
                    }

                    k[i] = k_value((size_t) num, (size_t) count[i]);
                    close(fd);
                }
                file_output(num, case_id, dir->name);
            }
        }
        sort_id = 0;
    }
}

int main(int argc, char *argv[])
{
    if (argc > 3) {
        printf("Too much arguments\n");
        return 1;
    }

    if (!strcmp(argv[1], "continuous")) {
        sort_test_continuously();
    } else if (!strcmp(argv[1], "single")) {
        if (argc < 3) {
            printf("Lack of given number for single node test\n");
            return 1;
        }
        int num = atoi(argv[2]);
        sort_test_one_num(num);
    } else {
        printf("Invalid argument %s\n", argv[1]);
        return 1;
    }
    return 0;
}
