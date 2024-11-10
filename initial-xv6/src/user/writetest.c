#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define ARRAY_SIZE 1024

int shared_memory[ARRAY_SIZE];

int main() {
    int pid = fork();
    if (pid < 0) {
        printf("Fork failed\n");
        exit(0);
    }
    
    if (pid == 0) {  // Child process
        for (int i = 0; i < 100; i++) {
            shared_memory[i % ARRAY_SIZE] = i;  // Modify memory, should trigger COW
        }
        // Retrieve and print the page fault count for the child
        // printf("Child (Write) Page Fault Count: %d\n", get_page_fault_count());
        exit(0);
    } else {  // Parent process
        wait(0);  // Wait for child to complete
    }

    exit(0);
}