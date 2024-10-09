#include <iostream>
#include <map>
#include <set>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <bits/stdc++.h>

// Custom mmap allocator
template <typename T>
struct MmapAllocator
{
    using value_type = T;

    MmapAllocator() {}

    template <typename U>
    MmapAllocator(const MmapAllocator<U>&) {}

    // Allocate memory using mmap
    T *allocate(std::size_t n) {
        void *ptr = mmap(nullptr, n * sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(ptr == MAP_FAILED) {
            std::cout << "Map allocation failed\n";
            throw std::bad_alloc();
        }
        return static_cast<T *>(ptr);
    }

    // Deallocate memory
    void deallocate(T *p, std::size_t n) {
        munmap(p, n * sizeof(T));
    }
};

// Data structure to hold the shared data in mmap
struct MapInStruct {
    std::map<int, std::set<int, std::less<int>, MmapAllocator<int>>, std::less<int>, MmapAllocator<std::pair<const int, std::set<int, MmapAllocator<int>>>>> ds;
};

// Structure to be stored in shared memory
struct SharedData {
    // MapInStruct arr[10];  // Array of map-sets
    std::vector<int>vec;
    int arr2[100];        // Example of a plain integer array
    int val;              // Example integer value
};

int main() {
    // Size of the shared memory block
    size_t SHARED_MEMORY_SIZE = sizeof(SharedData);

    // Create shared memory
    int fd = shm_open("/shared_data", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "Failed to create shared memory\n";
        return 1;
    }

    // Resize shared memory
    if (ftruncate(fd, SHARED_MEMORY_SIZE) == -1) {
        std::cerr << "Failed to set size of shared memory\n";
        return 1;
    }

    // Map shared memory to the process' address space
    SharedData *data = static_cast<SharedData*>(mmap(nullptr, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (data == MAP_FAILED) {
        std::cerr << "Failed to map shared memory\n";
        return 1;
    }

    // Fork the process
    pid_t pid = fork();
    sleep(1);  // Let the child process insert data

    if (pid == 0) {  // Child process
        std::cout << "Child Process\n";

        // Insert data into the map in shared memory
        data->vec.push_back(2);
        data->vec.push_back(3);
        data->vec.push_back(4);

        std::cout<<"Map insertion done\n";
        // data->arr[0].ds[1].insert(10);

        // // Confirm that data was inserted
        // if (data->arr[0].ds[1].find(10) != data->arr[0].ds[1].end()) {
        //     std::cout << "Data inserted in child\n";
        // }

        // Exit child process
        exit(0);
    } else {  // Parent process
        // Wait for the child process to finish
        sleep(3);
        std::cout<<data->vec[0];

        // Check if the data is present in the shared memory
        std::cout << "Data in parent process: " << data->vec[0] << std::endl;
        // if (data->arr[0].ds[1].find(10) != data->arr[0].ds[1].end()) {
        //     std::cout << "Data found in parent after child modification\n";
        // } else {
        //     std::cout << "Data not found in parent\n";
        // }

        // Clean up shared memory (optional)
        munmap(data, SHARED_MEMORY_SIZE);
        shm_unlink("/shared_data");
    }

    return 0;
}
