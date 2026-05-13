#include <chrono>
#include <iostream>
#include <thread>

int main() {
    std::cout << "Hello world!" << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }
}
