
#pragma once

#include <chrono>
#include <bitset>
#include <vector>

#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <cpuid.h>

struct TSCTimerHelper {
    static uint64_t average(uint64_t low, uint64_t high) {
        return low + (high - low) / 2;
    }

    static uint64_t rdtsc() {
        unsigned int A;
        return __builtin_ia32_rdtscp(&A);
    }

    static std::tuple<uint64_t, std::chrono::steady_clock::time_point> get_tsc_ns_pair() {
        constexpr int try_times = 5;
        uint64_t tsc_arr[try_times + 1];
        std::chrono::steady_clock::time_point ns_arr[try_times];

        tsc_arr[0] = rdtsc();
        for (int i = 0; i < try_times; ++i) {
            ns_arr[i] = std::chrono::steady_clock::now();
            tsc_arr[i + 1] = rdtsc();
        }

        auto min_tsc_diff = tsc_arr[1] - tsc_arr[0];
        auto ave_tsc = average(tsc_arr[0], tsc_arr[1]);
        auto ns = ns_arr[0];

        for (int i = 1; i < try_times; ++i) {
            auto tsc_diff = tsc_arr[i + 1] - tsc_arr[i];
            if (tsc_diff < min_tsc_diff) {
                min_tsc_diff = tsc_diff;
                ave_tsc = average(tsc_arr[i], tsc_arr[i + 1]);
                ns = ns_arr[i];
            }
        }

        return {ave_tsc, ns};
    }

    static void calibrate() {
        auto [end_tsc, end_time] = TSCTimerHelper::get_tsc_ns_pair();
        auto& [begin_tsc, begin_time] = TSCTimerHelper::base_point;
        std::chrono::nanoseconds duration = end_time - begin_time;
        cycle_to_ns_scale = static_cast<double>(duration.count()) / static_cast<double>(end_tsc - begin_tsc);
    }

    static std::tuple<uint64_t, std::chrono::steady_clock::time_point> base_point;
    static double cycle_to_ns_scale;
};

inline std::tuple<uint64_t, std::chrono::steady_clock::time_point> TSCTimerHelper::base_point = TSCTimerHelper::get_tsc_ns_pair();
inline double TSCTimerHelper::cycle_to_ns_scale = 0.;

template <int N, typename Period = std::ratio<1>, typename Rep = double>
class TSCTimer: protected TSCTimerHelper {
public:
    TSCTimer() {
        memset(&last_tsc_arr_, 0, sizeof(last_tsc_arr_));
        memset(&cycles_arr_, 0, sizeof(cycles_arr_));
    }

    template <typename... Ns>
    void track(Ns... track_types) {
        auto tsc = TSCTimerHelper::rdtsc();
        tsc_record_.push_back(tsc);
        constexpr auto num_types = sizeof...(Ns);
        int num_types_arr[num_types] = {track_types...};
        std::bitset<N> now_tracking;
        for (size_t i = 0; i < num_types; ++i) {
            now_tracking.set(num_types_arr[i]);
        }
        for (int i = 0; i < N; ++i) {
            if (now_tracking.test(i)) {
                if (not tracking_.test(i)) {
                    last_tsc_arr_[i] = tsc;
                }
            } else {
                if (tracking_.test(i)) {
                    cycles_arr_[i] += tsc - last_tsc_arr_[i];
                }
            }
        }
        tracking_ = now_tracking;
    }

    std::chrono::duration<Rep, Period> get(int i) {
        if (TSCTimerHelper::cycle_to_ns_scale == 0.) {
            TSCTimerHelper::calibrate();
        }
        std::chrono::nanoseconds result{static_cast<std::chrono::nanoseconds::rep>(cycles_arr_[i] * TSCTimerHelper::cycle_to_ns_scale)};
        return std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(result);
    }

    uint64_t get_cycle(int i) {
        return cycles_arr_[i];
    }

    void dump() {
        auto fp = fopen("tsc_record", "wb");
        fwrite(tsc_record_.data(), sizeof(uint64_t), tsc_record_.size(), fp);
        fclose(fp);
    }

private:
    uint64_t last_tsc_arr_[N];
    uint64_t cycles_arr_[N];
    std::bitset<N> tracking_;
    std::vector<uint64_t> tsc_record_;
};

template <int N, typename Period, typename Rep>
class TSCTimer<N, std::chrono::duration<Rep, Period>>: public TSCTimer<N, Period, Rep> {};