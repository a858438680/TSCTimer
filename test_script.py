#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os

if __name__ == '__main__':
    compile_command = 'g++ -O2 -std=c++20 -o a.out test.cpp'
    os.system(compile_command)
    for i in range(1000):
        f = os.popen("./a.out")
        times = [float(i.strip()) for i in f.readlines()]
        for t in times[1:]:
            if (t <= times[0] * 0.25):
                print(times)
                exit(1)
        print("test {} passed".format(i + 1))
    os.unlink("a.out")