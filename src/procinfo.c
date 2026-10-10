#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#define MAX_BUF 2048

int is_numeric(const char *str) {
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }
    return 1;
}

void print_error(const char *msg, int pid) {
    fprintf(stderr, "Error: %s (PID: %d)\n", msg, pid);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    if (!is_numeric(argv[1])) {
        fprintf(stderr, "Error: PID must be a positive integer.\n");
        return 1;
    }

    int pid = atoi(argv[1]);
    char path[256];
    char buffer[MAX_BUF];
    FILE *fp;

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp) {
        if (errno == ENOENT) fprintf(stderr, "Error: PID %d not found.\n", pid);
        else if (errno == EACCES) fprintf(stderr, "Error: Permission denied for PID %d.\n", pid);
        else perror("Error opening stat file");
        return 1;
    }

    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        perror("Error reading stat file");
        fclose(fp);
        return 1;
    }
    fclose(fp);

    char *last_paren = strrchr(buffer, ')');
    if (!last_paren || strlen(last_paren) < 4) {
        fprintf(stderr, "Error: Could not parse /proc/%d/stat\n", pid);
        return 1;
    }

    char state = last_paren[2];

    int ppid;
    unsigned long utime, stime;

    char *stat_values = last_paren + 4; 
    
    if (sscanf(stat_values, "%d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", 
               &ppid, &utime, &stime) != 3) {
        fprintf(stderr, "Error: Failed to extract data from stat file.\n");
        return 1;
    }
    
    double cpu_time_sec = (double)(utime + stime) / sysconf(_SC_CLK_TCK);

    char vmrss[64] = "N/A";
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    fp = fopen(path, "r");
    if (fp) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "VmRSS:", 6) == 0) {
                char *val = buffer + 6;
                while (*val && isspace(*val)) val++;

                strncpy(vmrss, val, sizeof(vmrss) - 1);
                vmrss[strcspn(vmrss, "\n")] = 0;
                break;
            }
        }
        fclose(fp);
    } 

    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    fp = fopen(path, "r");

    printf("Process State:   %c\n", state);
    printf("Parent PID:      %d\n", ppid);
    printf("CPU Time:        %.2f s\n", cpu_time_sec);
    printf("Resident Memory: %s\n", vmrss);
    printf("Command Line:    ");

    if (fp) {
        size_t n = fread(buffer, 1, sizeof(buffer) - 1, fp);
        if (n > 0) {
            for (size_t i = 0; i < n - 1; i++) {
                if (buffer[i] == '\0') printf(" ");
                else printf("%c", buffer[i]);
            }

            if (buffer[n-1] != '\0') printf("%c", buffer[n-1]);
        }
        fclose(fp);
    }
    printf("\n");

    return 0;
}
