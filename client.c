/* The code that test the sorting algorithms in kernel space
 */
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "sort.h"

#define SORT_DEV "/dev/sort_test"

#define LISTSORT "ls_data"
#define TIMMERGE "tm_data"
#define TIMLINEAR "tl_data"
#define TIMBINARY "tb_data"
#define TIMLGALLOP "tlg_data"
#define TIMBGALLOP "tbg_data"
#define ADAPSHIVER "ads_data"
#define ADSMERGE "adsm_data"

unsigned long long int duration[LOOP];
size_t count[LOOP];

typedef struct {
    char *name;
} directory;

directory dirs[] = {
    {.name = LISTSORT}, {.name = TIMMERGE}, {.name = TIMLINEAR}, 
    {.name = TIMBINARY}, {.name = TIMLGALLOP}, {.name = TIMBGALLOP}, 
    {.name = ADAPSHIVER}, {.name = ADSMERGE}, {.name = NULL}
};

static void file_output(size_t num, int case_id, char *dir_name)
{
    char cnt_file[100];
    char time_file[100];

    switch (case_id) {
    case 0: /* Worst case of merge sort */
        sprintf(cnt_file, "%s/w_count.txt", dir_name);
        sprintf(time_file, "%s/w_time.txt", dir_name);
        break;
    case 1: /* Random 3 elements */
        sprintf(cnt_file, "%s/r3_count.txt", dir_name);
        sprintf(time_file, "%s/r3_time.txt", dir_name);
        break;
    case 2: /* Random last 10 elements */
        sprintf(cnt_file, "%s/rl10_count.txt", dir_name);
        sprintf(time_file, "%s/rl10_time.txt", dir_name);
        break;
    case 3: /* Random 1% elements */
        sprintf(cnt_file, "%s/r1p_count.txt", dir_name);
        sprintf(time_file, "%s/r1p_time.txt", dir_name);
        break;
    case 4: /* Duplicate */
        sprintf(cnt_file, "%s/dup_count.txt", dir_name);
        sprintf(time_file, "%s/dup_time.txt", dir_name);
        break;
    default: /* Random elements */
        sprintf(cnt_file, "%s/r_count.txt", dir_name);
        sprintf(time_file, "%s/r_time.txt", dir_name);
        break;
    }

    FILE *cnt = fopen(cnt_file, "a");
    FILE *time = fopen(time_file, "a");

    if (!cnt) {
        perror("The output file `(.)_count.txt` might have been collapsed");
        exit(EXIT_FAILURE);
    }

    if (!time) {
        perror("The output file `(.)_time.txt` might have been collapsed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0 ; i < LOOP ; i++) {
        fprintf(cnt, "%lu %lu\n", num, count[i]);
        fprintf(time, "%lu %llu\n", num, duration[i]);
    }

    fclose(cnt);
    fclose(time);
}

static void sort_test_one_num(size_t num)
{
    if (num < MIN_LEN || num > MAX_LEN) {
        perror("Given number argument out of the range of number of nodes in this code");
        exit(EXIT_FAILURE);
    }
    
    int fd = open(SORT_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(EXIT_FAILURE);
    }
    
    int d = 0;
    for (int case_id = 0 ; case_id < 6 ; case_id++) {
        for (directory *dir = dirs ; dir->name ; d++, dir++) {
            for (int i = 0 ; i < LOOP ; i++) {
                st_dev st;
                st.nodes = num;
                st.case_id = case_id;
                st.test_case = d;
                /* write the properties of the current test case to device driver */
                if (write(fd, &st, sizeof(st)) < 0) {
                    perror("Failed to write to the device");
                    close(fd);
                    exit(EXIT_FAILURE);
                }

                printf("start to read\n");
                st_usr out_st;
                /* read the output of the test from device driver */
                if (read(fd, &out_st, sizeof(out_st)) < 0) {
                    perror("Failed to read from the device");
                    close(fd);
                    exit(EXIT_FAILURE);
                }

                count[i] = out_st.count;
                duration[i] = out_st.time;
            }

            file_output(num, case_id, dir->name);
        }

        d = 0;
    }
    close(fd);
}

static void sort_test_continuesly()
{
    int d = 0;
    for (int case_id = 0 ; case_id < 6 ; case_id++) {
        for (directory *dir = dirs ; dir->name ; d++, dir++) {
            for (int num = MIN_LEN ; num < MAX_LEN - 20 ; num++) {
                printf("start %s in #%d\n", dir->name, num);
                for (int i = 0 ; i < LOOP ; i++) {
                    int fd = open(SORT_DEV, O_RDWR);
                    if (fd < 0) {
                        perror("Failed to open character device");
                        exit(EXIT_FAILURE);
                    }

                    st_dev st;
                    st.nodes = num;
                    st.case_id = case_id;
                    st.test_case = d;
                    /* write the properties of the current test case to device driver */
                    if (write(fd, &st, sizeof(st)) < 0) {
                        perror("Failed to write to the device");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    st_usr out_st;
                    /* read the output of the test from device driver */
                    if (read(fd, &out_st, sizeof(out_st)) < 0) {
                        perror("Failed to read from the device");
                        close(fd);
                        exit(EXIT_FAILURE);
                    }

                    count[i] = out_st.count;
                    duration[i] = out_st.time;

                    close(fd);
                }
                file_output(num, case_id, dir->name);
                sleep(1);
            }
        }
        d = 0;
    }
}

int main()
{
    //sort_test_one_num(100);
    sort_test_continuesly();
    return 0;
}
