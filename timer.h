#pragma once

#include <atomic>
#include <bitset>
#include <chrono>
#include <vector>

#include <cstring>

namespace TSC {

struct TimerHelper {
    static uint64_t average(uint64_t low, uint64_t high) {
        return low + (high - low) / 2;
    }

    static uint64_t rdtscp() {
        unsigned int A;
        return __builtin_ia32_rdtscp(&A);
    }

    static uint64_t rdtsc() {
        return __builtin_ia32_rdtsc();
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
        auto [end_tsc, end_time] = TimerHelper::get_tsc_ns_pair();
        auto& [begin_tsc, begin_time] = TimerHelper::base_point;
        std::chrono::nanoseconds duration = end_time - begin_time;
        auto scale = static_cast<double>(duration.count()) / static_cast<double>(end_tsc - begin_tsc);
        double expected = 0.;
        cycle_to_ns_scale.compare_exchange_strong(expected, scale, std::memory_order_relaxed);
    }

    static std::tuple<uint64_t, std::chrono::steady_clock::time_point> base_point;
    static std::atomic<double> cycle_to_ns_scale;
};

inline std::tuple<uint64_t, std::chrono::steady_clock::time_point> TimerHelper::base_point = TimerHelper::get_tsc_ns_pair();
inline std::atomic<double> TimerHelper::cycle_to_ns_scale{0.};

struct RDTSC {};
struct RDTSCP {};

template <typename T>
struct is_ratio: std::false_type {};

template <std::intmax_t Num, std::intmax_t Denom>
struct is_ratio<std::ratio<Num, Denom>>: std::true_type {};

template <typename T>
concept Ratio = is_ratio<T>::value;

template <typename T>
concept Number = std::is_integral_v<T> || std::is_floating_point_v<T>;

template <typename T>
concept RDTBase = std::is_same_v<T, RDTSC> || std::is_same_v<T, RDTSCP>;

template <size_t N, Ratio Period = std::ratio<1>, Number Rep = double, RDTBase RDTType = RDTSC>
class Timer: protected TimerHelper {
public:
    Timer() {
        memset(&cycles_arr_, 0, sizeof(cycles_arr_));
    }

    struct end_tag_t {};

    template <typename... Ns>
    requires
        ((std::is_convertible_v<Ns, size_t> || std::is_same_v<Ns, end_tag_t>) && ...) &&
        (((std::is_same_v<end_tag_t, Ns> ? 1 : 0) + ...) <= 1) &&
        (((std::is_constructible_v<Ns, size_t> ? 1 : 0) + ...) <= N)
    void start(Ns... track_types) {
        ;
        if constexpr (std::is_same_v<RDTType, RDTSC>) {
            auto tsc = TimerHelper::rdtsc();
            start_impl(tsc, [](auto arg) constexpr {
                if constexpr (std::is_same_v<end_tag_t, decltype(arg)>) {
                    return arg;
                }   else {
                    return static_cast<size_t>(arg);
                }
            }(track_types)...);
        } else {
            auto tsc = TimerHelper::rdtscp();
            start_impl(tsc, [](auto arg) constexpr {
                if constexpr (std::is_same_v<end_tag_t, decltype(arg)>) {
                    return arg;
                }   else {
                    return static_cast<size_t>(arg);
                }
            }(track_types)...);
        }
    }

    template <typename... Ns>
    requires ((std::is_convertible_v<Ns, size_t>) && ...) && (sizeof...(Ns) <= N)
    void end(Ns... track_types) {
        if constexpr (std::is_same_v<RDTType, RDTSC>) {
            auto tsc = TimerHelper::rdtsc();
            end_impl(tsc, static_cast<size_t>(track_types)...);
        } else {
            auto tsc = TimerHelper::rdtscp();
            end_impl(tsc, static_cast<size_t>(track_types)...);
        }
    }

    std::chrono::duration<Rep, Period> get(size_t i) {
        auto scale = TimerHelper::cycle_to_ns_scale.load(std::memory_order_relaxed);
        if (scale == 0.) {
            TimerHelper::calibrate();
            while ((scale = TimerHelper::cycle_to_ns_scale.load(std::memory_order_relaxed)) == 0.);
        }
        std::chrono::nanoseconds result{static_cast<std::chrono::nanoseconds::rep>(cycles_arr_[i] * scale)};
        return std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(result);
    }

    static size_t size() {
        return N;
    }

    uint64_t get_cycle(size_t i) {
        return cycles_arr_[i];
    }

    static constexpr auto stop = end_tag_t{};

private:
    uint64_t last_tsc_arr_[N];
    uint64_t cycles_arr_[N];

    template <typename... Ns>
    void start_impl(uint64_t tsc, size_t first_start_type, Ns... start_types) {
        last_tsc_arr_[first_start_type] = tsc;
        start_impl(tsc, start_types...);
    }

    void start_impl(uint64_t tsc) {}

    template <typename... Ns>
    void start_impl(uint64_t tsc, end_tag_t, Ns... end_types) {
        end_impl(tsc, end_types...);
    }

    template <typename... Ns>
    void end_impl(uint64_t tsc, size_t first_start_type, Ns... start_types) {
        cycles_arr_[first_start_type] += tsc - last_tsc_arr_[first_start_type];
        end_impl(tsc, start_types...);
    }

    void end_impl(uint64_t tsc) {}
};

} // namespace TSC