# TSCTimer

---

## What is TSCTimer?

TSCTimer是一个低延时高精度的计时器，但是只能运行在Intel平台上的Linux操作系统中，因为它使用读取TSC寄存器来进行计时，省去了系统调用的开销。

---

## What is TSC (Time Stamp Counter)?

TSC是x86平台上的一种计数器，这个计数器的数值随着时间不断增长，在一些Intel平台上，TSC能够保证在同一个主板的不同CPU的不同核之间同步，并且其增长速率不随着CPU Boost（睿频）或电源状态（休眠等）的改变而改变。

[这篇文章](http://oliveryang.net/2015/09/pitfalls-of-TSC-usage/)比较详尽的描述了使用TSC的各种细节和痛点。

---

## Can I use TSCTimer?

1. 确保你的CPU是Intel的。
2. 确保你的操作系统是Linux。
3. 确保你的CPU支持Invariant TSC特性，执行下面的命令，如果在输出的内容中同时看到了`constant_tsc`和`nonstop_tsc`证明你的CPU支持Invariant TSC特性。（如果你的CPU是AMD的，即使满足这条要求也不可以使用TSCTimer。）
```
cat /proc/cpuinfo | grep -E "constant_tsc|nonstop_tsc"
```

## How can I use TSCTimer?

请参考`test.cpp`，这里再给出另一个例子，我们要记录5部分的代码，

0. 外面整个循环的代码
1. foo()的调用时间
2. 内部循环的代码
3. bar1()的调用时间
4. bar2()的调用时间

其中3, 4加起来和2接近，1, 2加起来和0接近。

```c++
TSCTimer<5> timer;

timer.track(0);
for (int i = 0; i < 100; ++i) {
    timer.track(0, 1);
    foo()
    timer.track(0, 2);
    for (int j = 0; j < 100; ++j) {
        timer.track(0, 2, 3);
        bar1();
        timer.track(0, 2, 4);
        bar2();
        timer.track(0, 2);
    }
    timer.track(0);
}
timer.track();

for (size_t i = 0; i < timer.size(); ++i) {
    std::cout << i << ": " << timer.get(i).count() << std::endl;
}
```

TSCTimer默认使用`std::chrono::duration<double, std::ratio<1>>` 来进行输出，如果你想使用`chrono`提供的其他秒数类型也是很容易的：

```c++
TSCTimer<5, std::chrono::seconds> timer1;
TSCTimer<5, std::chrono::milliseconds> timer2;
TSCTimer<5, std::chrono::microseconds> timer3;
TSCTimer<5, std::chrono::nanoseconds> timer4;
```

如果你依然想使用`double`进行输出，可以直接指定单位的比例：

```c++
TSCTimer<5, std::milli> timer1;
TSCTimer<5, std::micro> timer2;
TSCTimer<5, std::nano> timer3;
```

如果你想同时指定单位和数据类型，也是可以的，而且有两种方式：

```c++
TSCTimer<5, std::micro, float> timer1;
TSCTimer<5, std::chrono::duration<float, std::micro>> timer2; // same as timer1
```
---

## How does TSCTimer work?

1. 在使用了TSCTimer的程序初始化时就会自动记录TSC的寄存器值和对应的时间。
2. 在首次调用`TSCTimer::get(int)`成员函数的的时候，会检查时候保存了用于转换TSC值到时间的比例，如果没有，调用`TSCTimerHelper::calibrate()`函数来记录当前的TSC的寄存器值和时间。和程序初始化时记录的时间联合起来，就可以计算出TSC到真是时间的转换比例。
3. 这个转换比例是全局唯一的，因此，程序的同一次运行时，不同的TSCTimer实例输出的时间是完全可以比较的。
4. 在程序的多次运行之间，输出的时间仍然是基本可比的，1s左右的calibrate时间所记录的比例精度大约已经在10e-6的数量级上，如果程序记录的总时间更长，一般来说这种calibrate的方法已经完全足够精确了。

---

## Is TSCTimer thread-safe?

TSCTimer不是thread-safe的，同一个TSCTimer不能在不同线程间访问。

但是不同的TSCTimer可能会同时想要写入全局的转换比例，这个写入是线程安全的。因此可以放心的在不同线程间构造多个TSCTimer实例以用于计时。