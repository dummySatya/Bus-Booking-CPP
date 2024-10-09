#include <iostream>
#include <unistd.h>

static int counter = 0; 

int main() {
    std::cout << "Initial Counter: " << counter << std::endl;

    pid_t pid = fork();

    if (pid == 0) {
        counter += 5;
        std::cout << "Child Process Counter: " << counter << std::endl;
    } else {
        counter += 10;
        std::cout << "Parent Process Counter: " << counter << std::endl;
    }

    return 0;
}
