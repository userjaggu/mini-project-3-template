#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <ctype.h>

#define NUM_THREADS 4
#define MAX_NAME_LENGTH 128
#define TIMESTAMP_LENGTH 20

pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char name[MAX_NAME_LENGTH];
    int id;
    char timestamp[TIMESTAMP_LENGTH];
} File;

typedef struct {
    File file;
    long long hash_value;
} FileHash;

typedef struct {
    FileHash* file_hashes;
    int start_idx;
    int end_idx;    
    char sort_by;
} ThreadData;

// Converting timestamp to comparable number
long long timestamp_to_number(const char* timestamp) {
    int year, month, day, hour, minute, second;
    sscanf(timestamp, "%d-%d-%dT%d:%d:%d", 
           &year, &month, &day, &hour, &minute, &second);
    
    return (long long)year * 100000000000LL + 
           (long long)month * 1000000000LL + 
           (long long)day * 10000000LL + 
           (long long)hour * 100000LL + 
           (long long)minute * 1000LL + 
           (long long)second;
}

// Calculating hash for name to maintain lexicographical order
long long hash_name(const char* name) {
    long long hash = 0;
    int len = strlen(name);
    for(int i = 0; i < len && i < 8; i++) {
        hash = hash * 256 + (unsigned char)name[i];
    }
    return hash;
}

void* calculate_hashes(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    for(int i = data->start_idx; i < data->end_idx; i++) {
        switch(data->sort_by) {
            case 'I':
                data->file_hashes[i].hash_value = data->file_hashes[i].file.id;
                break;
            case 'N':
                data->file_hashes[i].hash_value = hash_name(data->file_hashes[i].file.name);
                break;
            case 'T':
                data->file_hashes[i].hash_value = timestamp_to_number(data->file_hashes[i].file.timestamp);
                break;
        }
    }
    
    pthread_exit(NULL);
}

void counting_sort(FileHash* arr, int size) {
    long long min_val = LLONG_MAX;
    long long max_val = LLONG_MIN;
    
    // Finding min and max hash values
    for (int i = 0; i < size; i++) {
        if (arr[i].hash_value < min_val) min_val = arr[i].hash_value;
        if (arr[i].hash_value > max_val) max_val = arr[i].hash_value;
    }
    
    long long range = max_val - min_val + 1;
    int* count = (int*)calloc(range, sizeof(int));
    FileHash* output = (FileHash*)malloc(size * sizeof(FileHash));
    
    if (!count || !output) {
        printf("Memory allocation failed\n");
        free(count);
        free(output);
        return;
    }
    
    // Counting frequencies with a mutex lock
    for (int i = 0; i < size; i++) {
        pthread_mutex_lock(&count_mutex);
        count[arr[i].hash_value - min_val]++;
        pthread_mutex_unlock(&count_mutex);
    }
    
    // Cumulative sum to get positions
    for (int i = 1; i < range; i++) {
        count[i] += count[i - 1];
    }
    
    // Placing elements in sorted order with a mutex lock
    for (int i = size - 1; i >= 0; i--) {
        long long adjusted_value = arr[i].hash_value - min_val;
        pthread_mutex_lock(&count_mutex);
        output[count[adjusted_value] - 1] = arr[i];
        count[adjusted_value]--;
        pthread_mutex_unlock(&count_mutex);
    }
    
    memcpy(arr, output, size * sizeof(FileHash));
    
    free(count);
    free(output);
}

void parallel_sort(File* files, int num_files, char sort_by) {
    FileHash* file_hashes = malloc(num_files * sizeof(FileHash));
    if (!file_hashes) {
        printf("Memory allocation failed\n");
        return;
    }
    
    for(int i = 0; i < num_files; i++) {
        file_hashes[i].file = files[i];
    }
    
    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];
    
    int chunk_size = (num_files + NUM_THREADS - 1) / NUM_THREADS;
    
    for(int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].file_hashes = file_hashes;
        thread_data[i].start_idx = i * chunk_size;
        thread_data[i].end_idx = (i == NUM_THREADS - 1) ? num_files : 
                                 (i + 1) * chunk_size > num_files ? num_files : (i + 1) * chunk_size;
        thread_data[i].sort_by = sort_by;
        
        if (thread_data[i].start_idx >= num_files) {
            continue;
        }
        
        if (pthread_create(&threads[i], NULL, calculate_hashes, &thread_data[i]) != 0) {
            printf("Failed to create thread %d\n", i);
            free(file_hashes);
            return;
        }
    }
    
    for(int i = 0; i < NUM_THREADS; i++) {
        if (thread_data[i].start_idx < num_files) {
            pthread_join(threads[i], NULL);
        }
    }
    
    counting_sort(file_hashes, num_files);
    
    for(int i = 0; i < num_files; i++) {
        files[i] = file_hashes[i].file;
    }
    
    free(file_hashes);
}

void print_files(File* files, int num_files) {
    printf("\nSorted Files:\n");
    printf("%-30s %-5s %-20s\n", "Name", "ID", "Timestamp");
    printf("--------------------------------------------------------\n");
    for(int i = 0; i < num_files; i++) {
        printf("%-30s %-5d %-20s\n", 
               files[i].name, files[i].id, files[i].timestamp);
    }
}

int main() {
    int num_files;
    char sort_column[20];
    File* files;
    
    pthread_mutex_init(&count_mutex, NULL);
    
    printf("Enter the number of files: ");
    if (scanf("%d", &num_files) != 1 || num_files <= 0) {
        printf("Invalid number of files\n");
        return 1;
    }
    
    files = (File*)malloc(num_files * sizeof(File));
    if (!files) {
        printf("Memory allocation failed\n");
        return 1;
    }

    printf("Enter file details (name id timestamp) for each file:\n");
    for(int i = 0; i < num_files; i++) {
        if (scanf("%s %d %s", files[i].name, &files[i].id, files[i].timestamp) != 3) {
            printf("Invalid input format\n");
            free(files);
            return 1;
        }
    }
    
    printf("Enter sorting column (Name, ID, or Timestamp): ");
    scanf("%s", sort_column);
    
    sort_column[0] = toupper(sort_column[0]);
    clock_t start , end;
    start = clock();
    if(sort_column[0] == 'I' || sort_column[0] == 'N' || sort_column[0] == 'T') {
        parallel_sort(files, num_files, sort_column[0]);
        print_files(files, num_files);
    } else {
        printf("Invalid sorting column. Please use Name, ID, or Timestamp.\n");
    }
    
    pthread_mutex_destroy(&count_mutex);
    end = clock();
   double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("%f",cpu_time_used);
    free(files);
    return 0;
}
