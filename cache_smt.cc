#include <cinttypes>
#include <iostream>
#include <array>
#include <iterator>
#include <random>
#include <algorithm>
#include <chrono>
#include <cassert>
#include <atomic>
#include <thread>
#include <vector>
#include <future>


#define CacheSize (1<<(23+1))
#define CacheLineSize (1<<6)

#define IntSize 4
#define ArraySize (CacheSize/IntSize)
#define IndexArraySize (ArraySize)
#define PayloadArraySize (ArraySize*4)

#define IntPerLine (CacheLineSize/IntSize)

#define InnerLoopCount 1024

#define NumOuterLoop (IndexArraySize - InnerLoopCount)

std::array<int, PayloadArraySize> payload;

std::array<int, IndexArraySize> indices;

std::array<int, ArraySize> flusher;

static std::random_device rd;    // you only need to initialize it once
static std::mt19937 mte(rd());   // this is a relative big object to create

int flush_cache()
{
    return std::accumulate(flusher.begin(), flusher.end(), 0);
}

template< class Iter >
void fill_with_random_int_values( Iter start, Iter end, int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    std::generate(start, end, [&] () { return dist(mte); });
}

std::atomic<int> index_comm;
std::atomic<int> count;

void gather_thread(int local_count_init, std::promise<int> && p)
{
    int sum = 0;
    int local_count = local_count_init;
    for (int i = local_count_init; i < NumOuterLoop; i += 2) {
        int index = indices[i];
        int tmp = 0;
        for (int x = 0; x < InnerLoopCount; x++) {
            tmp += payload[indices[i+x]];
        }
        // if (i < 10 || i > IndexArraySize - 1034) {
        //     std::cout << tmp << std::endl;
        // }
        while (count.load() != local_count) {
        }

        int index_offset = index_comm.load();
        int real_index = std::abs(index + index_offset) % PayloadArraySize;
        index_offset +=
            tmp +
            payload[real_index];

        index_comm.store(index_offset);

        local_count = ++count + 1;

        // if (i < 10 || i > IndexArraySize - 1034) {
        //     std::cout << real_index << std::endl;
        // }

        sum += index_offset;
    }
    p.set_value(sum);
}

int parallel_gather(){
    index_comm.store(0);
    count.store(0);

    static const int num_threads = 2;
    std::vector<std::thread> threads;
    std::vector<int> sums(num_threads);
    std::vector<std::promise<int>> promises(num_threads);
    std::vector<std::future<int>> futures(num_threads);


    for (int i = 0; i < num_threads; i++) {
        futures[i] = promises[i].get_future();
        threads.emplace_back(gather_thread, i, std::move(promises[i]));
    }

    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    // std::cout << futures[0].get() << std::endl;
    // std::cout << futures[1].get() << std::endl;
    return futures[0].get() + futures[1].get();
}

int gather()
{
    int sum = 0;
    int index_offset = 0;
    for (int i = 0; i < NumOuterLoop; i++) {
        int index = indices[i];
        int real_index = std::abs(index + index_offset) % PayloadArraySize;
        //std::cout << real_index << std::endl;
        int tmp = 0;
        for (int x = 0; x < InnerLoopCount; x++) {
            tmp += payload[indices[i+x]];
        }

        // if (i < 10 || i > IndexArraySize - 1034) {
        //     std::cout << tmp << std::endl;
        // }

        index_offset +=
            tmp +
            payload[real_index];

        // if (i < 10 || i > IndexArraySize - 1034) {
        //     std::cout << real_index << std::endl;
        // }

        sum += index_offset;
    }
    return sum;
}

int seq_sum()
{
    int sum = 0;
    for (auto index: indices) {
        sum += index;
    }
    return sum;
}



int main(int argc, char *argv[])
{
    // generate payload
    fill_with_random_int_values(payload.begin(), payload.end(), -10000, 10000);
    // generate useless flusher
    fill_with_random_int_values(flusher.begin(), flusher.end(), -10000, 10000);

    fill_with_random_int_values(indices.begin(), indices.end(), 0, PayloadArraySize - 1);

    auto useless = flush_cache();

    using namespace std::chrono;

    // {
    // // seq:
    // auto start = high_resolution_clock::now();
    // int sum = seq_sum();
    // auto end   = high_resolution_clock::now();
    // auto duration = duration_cast<microseconds>(end - start);

    // std::cout << "The number of iterations of outer loop: " << NumOuterLoop
    //     << std::endl;

    // std::cout << "It takes "
    //     << double(duration.count()) * microseconds::period::num /
    //     microseconds::period::den<< " seconds to read "
    //     << ArraySize << " sequential elements"
    //     << std::endl;
    // std::cout << "sum: " << sum
    //     << ", useless: " << useless
    //     << std::endl;
    // }

    useless = flush_cache();

    int old_sum;
    {
    // scattered:
    auto start = high_resolution_clock::now();
    int sum = gather();
    auto end   = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    std::cout << "It takes "
        << double(duration.count()) * microseconds::period::num /
        microseconds::period::den<< " seconds to read "
        << (int64_t) NumOuterLoop * (1 + InnerLoopCount) << " scattered elements"
        << std::endl;
    std::cout << "sum: " << sum
        << ", useless: " << useless
        << std::endl;
    old_sum = sum;
    }

    // {
    // // scattered, in parallel:
    // auto start = high_resolution_clock::now();
    // int sum = parallel_gather();
    // auto end   = high_resolution_clock::now();
    // auto duration = duration_cast<microseconds>(end - start);

    // std::cout << "It takes "
    //     << double(duration.count()) * microseconds::period::num /
    //     microseconds::period::den<< " seconds to read "
    //     << (int64_t) NumOuterLoop * (1 + InnerLoopCount) << " scattered elements in parallel"
    //     << std::endl;
    // std::cout << "sum: " << sum
    //     << ", useless: " << useless
    //     << std::endl;
    // assert(sum == old_sum);

    // }
    return 0;
}
