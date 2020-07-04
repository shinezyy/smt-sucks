#include <cinttypes>
#include <iostream>
#include <array>
#include <iterator>
#include <random>
#include <algorithm>
#include <chrono>


#define CacheSize (1<<(23+1))
#define CacheLineSize (1<<6)

#define IntSize 4
#define ArraySize (CacheSize/IntSize)
#define IndexArraySize (ArraySize*10)
#define BigArraySize (ArraySize*4)

std::array<int, BigArraySize> payload;

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

int gather()
{
    int sum = 0;
    for (auto index: indices) {
        sum += payload[index];
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

    fill_with_random_int_values(indices.begin(), indices.end(), 0, BigArraySize - 1);

    auto useless = flush_cache();

    using namespace std::chrono;

    {
    // seq:
    auto start = high_resolution_clock::now();
    int sum = seq_sum();
    auto end   = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    std::cout << "It takes "
        << double(duration.count()) * microseconds::period::num /
        microseconds::period::den<< " seconds to read "
        << ArraySize << " sequential elements"
        << std::endl;
    std::cout << "sum: " << sum
        << ", useless: " << useless
        << std::endl;
    }

    {
    // scattered:
    auto start = high_resolution_clock::now();
    int sum = gather();
    auto end   = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    std::cout << "It takes "
        << double(duration.count()) * microseconds::period::num /
        microseconds::period::den<< " seconds to read "
        << ArraySize << " scattered elements"
        << std::endl;
    std::cout << "sum: " << sum
        << ", useless: " << useless
        << std::endl;
    }




    return 0;
}
