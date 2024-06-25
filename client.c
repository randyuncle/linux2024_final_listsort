/* The code that test the sorting algorithms in kernel space
 */
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sort.h"

#define SORT_DEV "/dev/sort_test"

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

    FILE *cnt = fopen("data/data_count.txt", "w");
    FILE *time = fopen("data/data_time.txt", "w");

    /* Testing mode. In reality, it should have 6 cases for testing */
    for (int case_id = 0 ; case_id < 1 ; case_id++) {
        st_dev st;
        st.nodes = num;
        st.case_id = case_id;

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

        for (int i = 0 ; i < LOOP ; i++) {
            fprintf(cnt, "%d %lu\n", i + 1, out_st.count[i]);
            fprintf(time, "%d %llu\n", i + 1, out_st.time[i]);
        }
    }

    close(fd);
    fclose(time);
    fclose(cnt);
    
}

// static void sort_test_progress(size_t num)
// {
    
// }

int main()
{
    sort_test_one_num(20000);

    return 0;
}
