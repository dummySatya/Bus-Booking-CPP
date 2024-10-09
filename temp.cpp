#include <iostream>
#include <unistd.h>



int main() {
    int numChildren = 3;
    CommonData::sharedData = 1;
    for (int i = 0; i < numChildren; ++i) {
        sleep(1);
        pid_t pid = fork();
        if (pid == 0) {
            CommonData::sharedData += 1;
            std::cout << "Child " << i << " sees sharedData = " << CommonData::sharedData << std::endl;
            return 0;
        } 
    }
    return 0;
}
