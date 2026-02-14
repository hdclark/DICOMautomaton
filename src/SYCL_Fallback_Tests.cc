// SYCL_Fallback_Tests.cc -- Written by hal clark, 2026.

#include <vector>
#include <numeric>
#include <iostream>
#include <atomic>
#include <set>
#include <mutex>
#include <thread>

#include "doctest20251212/doctest.h"

#include "SYCL_Fallback.h"


TEST_CASE("SYCL 1D: Simple Vector Scaling") {
    const int N = 64;
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0); // 0, 1, 2...

    sycl::queue q;
    {
        sycl::buffer<int, 1> buf(data.data(), sycl::range<1>(N));
        
        q.submit([&](sycl::handler& h) {
            sycl::accessor acc(buf, h);
            h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> idx) {
                acc[idx] *= 2;
            });
        });
    }

    CHECK(data[0] == 0);
    CHECK(data[1] == 2);
    CHECK(data[10] == 20);
    CHECK(data[N-1] == (N-1) * 2);
}

TEST_CASE("SYCL 2D: Matrix Indexing") {
    const int ROWS = 4;
    const int COLS = 8;
    std::vector<int> matrix(ROWS * COLS, 0);

    sycl::queue q;
    sycl::buffer<int, 2> buf(matrix.data(), sycl::range<2>(ROWS, COLS));

    q.submit([&](sycl::handler& h) {
        sycl::accessor acc(buf, h);
        h.parallel_for(sycl::range<2>(ROWS, COLS), [=](sycl::id<2> idx) {
            // idx[0] is row, idx[1] is col
            acc[idx] = static_cast<int>(idx[0] * 10 + idx[1]);
        });
    });

    // Verify row 2, col 5
    CHECK(matrix[2 * COLS + 5] == 25);
    // Verify row 3, col 7
    CHECK(matrix[3 * COLS + 7] == 37);
}

TEST_CASE("SYCL 3D: Volumetric Indexing") {
    const int D = 2, H = 3, W = 4;
    std::vector<int> volume(D * H * W, 0);

    sycl::queue q;
    sycl::buffer<int, 3> buf(volume.data(), sycl::range<3>(D, H, W));

    q.submit([&](sycl::handler& h) {
        sycl::accessor acc(buf, h);
        h.parallel_for(sycl::range<3>(D, H, W), [=](sycl::id<3> idx) {
            acc[idx] = 1;
        });
    });

    // Check a few points
    CHECK(volume[0] == 1);
    CHECK(volume[D * H * W - 1] == 1);
    // Verify sum is correct to ensure every point was hit exactly once
    int sum = std::accumulate(volume.begin(), volume.end(), 0);
    CHECK(sum == (D * H * W));
}

TEST_CASE("SYCL Kernel Signatures: item vs id") {
    const int N = 10;
    std::vector<int> data_id(N, 0);
    std::vector<int> data_item(N, 0);

    sycl::queue q;
    sycl::buffer<int, 1> buf_id(data_id.data(), sycl::range<1>(N));
    sycl::buffer<int, 1> buf_item(data_item.data(), sycl::range<1>(N));

    // Test with sycl::id signature
    q.submit([&](sycl::handler& h) {
        sycl::accessor acc(buf_id, h);
        h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
            acc[i] = 100;
        });
    });

    // Test with sycl::item signature
    q.submit([&](sycl::handler& h) {
        sycl::accessor acc(buf_item, h);
        h.parallel_for(sycl::range<1>(N), [=](sycl::item<1> it) {
            acc[it.get_id()] = 200;
        });
    });

    CHECK(data_id[0] == 100);
    CHECK(data_item[0] == 200);
}

TEST_CASE("SYCL Linear ID Calculation") {
    // Testing the logic inside sycl::item for linear ID mapping
    sycl::range<2> r(10, 20); // 10 rows, 20 cols
    sycl::id<2> i(2, 5);      // Row 2, Col 5
    sycl::item<2> it(r, i);

    // Row-major: 2 * 20 + 5 = 45
    CHECK(it.get_linear_id() == 45);
}

TEST_CASE("SYCL 1D: Each Index Executed Exactly Once") {
    const size_t N = 4096;
    std::vector<std::atomic<int>> counts(N);
    for(auto &v : counts){
        v.store(0);
    }

    sycl::queue q;
    q.parallel_for(sycl::range<1>(N), [&](sycl::id<1> idx) {
        counts[idx[0]].fetch_add(1, std::memory_order_relaxed);
    });

    size_t n_incorrect = 0;
    for(const auto &v : counts){
        if(v.load(std::memory_order_relaxed) != 1){
            ++n_incorrect;
        }
    }
    CHECK(n_incorrect == 0U);
}

TEST_CASE("SYCL 2D: parallel_for Direct Queue API") {
    const size_t rows = 32;
    const size_t cols = 64;
    std::vector<int> matrix(rows * cols, 0);

    sycl::queue q;
    q.parallel_for(sycl::range<2>(rows, cols), [&](sycl::id<2> idx) {
        matrix[idx[0] * cols + idx[1]] = static_cast<int>((idx[0] + 1U) * (idx[1] + 1U));
    });

    CHECK(matrix[0] == 1);
    CHECK(matrix[(rows - 1U) * cols + (cols - 1U)] == static_cast<int>(rows * cols));
}

TEST_CASE("SYCL Parallel Execution Uses Multiple Worker Threads") {
    const size_t N = 8192;
    std::set<std::thread::id> thread_ids;
    std::mutex thread_ids_mutex;

    sycl::queue q;
    q.parallel_for(sycl::range<1>(N), [&](sycl::id<1>) {
        std::lock_guard<std::mutex> lock(thread_ids_mutex);
        thread_ids.insert(std::this_thread::get_id());
    });

    CHECK(thread_ids.size() >= 1U);
    if(std::thread::hardware_concurrency() > 1U){
        CHECK(thread_ids.size() > 1U);
    }
}

TEST_CASE("SYCL Sampled Image Nearest and Linear Modes") {
    std::vector<float> data = {
        0.0f, 10.0f,
        20.0f, 30.0f
    }; // depth=1, height=2, width=2, channels=1

    sycl::image_sampler nearest_sampler(
        sycl::coordinate_normalization_mode::unnormalized,
        sycl::addressing_mode::clamp_to_edge,
        sycl::filtering_mode::nearest
    );
    sycl::sampled_image<float, 3> nearest_img(data.data(), 2, 2, 1, 1, nearest_sampler);
    CHECK(nearest_img.read(1.0, 1.0, 0.0).x == doctest::Approx(30.0f));

    sycl::image_sampler linear_sampler(
        sycl::coordinate_normalization_mode::unnormalized,
        sycl::addressing_mode::clamp_to_edge,
        sycl::filtering_mode::linear
    );
    sycl::sampled_image<float, 3> linear_img(data.data(), 2, 2, 1, 1, linear_sampler);
    CHECK(linear_img.read(0.5, 0.5, 0.0).x == doctest::Approx(15.0f));
}
