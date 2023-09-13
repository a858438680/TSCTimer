#include <random>
#include <vector>
#include <iostream>

#include "timer.h"

int main() {
    TSC::Timer<4, std::milli> timer;

    std::vector<int> vec1;
    std::vector<int> vec2;
    std::vector<int> vec3;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis1(1, 100);
    std::uniform_int_distribution<> dis2(101, 200);
    std::uniform_int_distribution<> dis3(201, 300);

    timer.start(0);
    for (int i = 0; i < 1000000; ++i) {
        timer.start(1);
        for (int j = 0; j < 100; ++j) {
            vec1.push_back(dis1(gen));
        }
        timer.start(2, timer.stop, 1);
        for (int j = 0; j < 100; ++j) {
            vec2.push_back(dis2(gen));
        }
        timer.start(3, timer.stop, 2);
        for (int j = 0; j < 100; ++j) {
            vec3.push_back(dis3(gen));
        }
        timer.end(3);
    }
    timer.end(0);

    // std::cout << std::accumulate(vec1.begin(), vec1.end(), 0) << std::endl;
    // std::cout << std::accumulate(vec2.begin(), vec2.end(), 0) << std::endl;
    // std::cout << std::accumulate(vec3.begin(), vec3.end(), 0) << std::endl;

    for (int i = 0; i < timer.size(); ++i) {
        std::cout << timer.get(i) << std::endl;
    }
}