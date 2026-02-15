// SYCL_Fallback.h.

// This is a *mock*, minimal, CPU-only implementation of SYCL.hpp.
// It is meant to help compile and run SYCL code when the compiler or toolchain lacks support.
// Code compiled with this mock header will NOT have any runtime support.
// Based on the SYCL 2020 standard (but missing a lot of functionality!).

#ifndef SYCL_FALLBACK_HPP
#define SYCL_FALLBACK_HPP

#include <vector>
#include <array>
#include <functional>
#include <cstddef>
#include <algorithm>
#include <memory>
#include <iostream>
#include <type_traits> // Required for std::enable_if and std::is_integral
#include <exception>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Thread_Pool.h"

namespace sycl {

// =============================================================================
// Enums for template tagging.
// =============================================================================
namespace info {
    enum class device : int {
        name,
        vendor,
        version
    };
}

namespace access {
    enum class mode { read, write, read_write, discard_write, discard_read_write, atomic };
    enum class target { global_buffer, constant_buffer, local, image, host_buffer };
    enum class placeholder { false_t, true_t };
}


// =============================================================================
// Forward declarations
// =============================================================================
template <int Dims> struct range;
template <int Dims> struct id;
template <int Dims> struct item;

template <typename T, int Dims> class buffer;
template <typename T, int Dims, access::mode Mode, access::target Target> class accessor;

class handler;
class queue;
class device;
struct default_selector_t {};
inline constexpr default_selector_t default_selector_v{};
using exception_list = std::vector<std::exception_ptr>;



// =============================================================================
// Basic Identifiers: id, range, item
// =============================================================================

template <int Dims>
struct range {
    std::array<size_t, Dims> dims;

    // Use SFINAE to ensure we only catch integer types
    template<typename... Args, 
             typename = std::enable_if_t<(std::is_integral_v<Args> && ...)>>
    range(Args... args) : dims{static_cast<size_t>(args)...} {}

    range(std::initializer_list<size_t> list) {
        std::copy(list.begin(), list.end(), dims.begin());
    }

    size_t operator[](int i) const { return dims[i]; }
    size_t size() const {
        size_t s = 1;
        for (auto d : dims) s *= d;
        return s;
    }
};

template <int Dims>
struct id {
    std::array<size_t, Dims> val;

    id() : val{} {}
    
    // Use SFINAE to ensure we only catch integer types
    template<typename... Args, 
             typename = std::enable_if_t<(std::is_integral_v<Args> && ...)>>
    id(Args... args) : val{static_cast<size_t>(args)...} {}
    
    // Allow implicit conversion from array for internal loop logic
    id(const std::array<size_t, Dims>& arr) : val(arr) {}

    size_t operator[](int i) const { return val[i]; }
};



template <int Dims>
struct item {
    range<Dims> r;
    id<Dims> i;

    item(range<Dims> range_val, id<Dims> id_val) 
        : r(range_val), i(id_val) {}

    id<Dims> get_id() const { return i; }
    range<Dims> get_range() const { return r; }
    size_t get_linear_id() const {
        size_t idx = 0;
        size_t stride = 1;
        // Standard row-major linearization
        for (int d = Dims - 1; d >= 0; --d) {
            idx += i[d] * stride;
            stride *= r[d];
        }
        return idx;
    }
};

class device {
public:
    // SYCL 2020 get_info uses template specialization
    template <info::device Param>
    auto get_info() const {
        if constexpr (Param == info::device::name) {
            return std::string("DICOMautomaton host-only fallback device");
        } else if constexpr (Param == info::device::vendor) {
            return std::string("DICOMautomaton");
        }
        return std::string("DICOMautomaton");
    }
};

// =============================================================================
// Memory Model: buffer, accessor
// =============================================================================

// Minimal buffer: Manages ownership or wraps existing pointers
template <typename T, int Dims = 1>
class buffer {
    std::shared_ptr<std::vector<T>> data_storage; // Used if we own data
    T* host_ptr = nullptr;
    range<Dims> r;
    bool use_host_ptr = false;

public:
    // Constructor: Owns data
    buffer(range<Dims> r) : r(r), data_storage(std::make_shared<std::vector<T>>(r.size())) {}

    // Constructor: Wraps host pointer
    buffer(T* data, range<Dims> r) : host_ptr(data), r(r), use_host_ptr(true) {}

    // Internal helper to get raw pointer
    T* get_pointer() const {
        return use_host_ptr ? host_ptr : data_storage->data();
    }

    range<Dims> get_range() const { return r; }
};

// Accessor: The view into the buffer
template <typename T, int Dims = 1, 
          access::mode Mode = access::mode::read_write, 
          access::target Target = access::target::global_buffer>
class accessor {
    T* ptr;
    range<Dims> r;

public:
    // Update: Accept a reference to the handler
    accessor(buffer<T, Dims>& buf, handler& /*h*/) 
        : ptr(buf.get_pointer()), r(buf.get_range()) {
        //h.require(*this); // Register the dependency (even if it's a no-op)
    }

    // 1D Access
    T& operator[](id<1> index) const {
        return ptr[index[0]];
    }
    T& operator[](size_t index) const {
        return ptr[index];
    }

    // 2D Access
    T& operator[](id<2> index) const {
        return ptr[index[0] * r[1] + index[1]];
    }

    // 3D Access
    T& operator[](id<3> index) const {
        return ptr[index[0] * (r[1] * r[2]) + index[1] * r[2] + index[2]];
    }
};

// =============================================================================
// Deduction Guides (Required for C++17 CTAD)
// =============================================================================

template <typename T, int Dims>
accessor(buffer<T, Dims>&, handler&) -> accessor<T, Dims, access::mode::read_write, access::target::global_buffer>;


// =============================================================================
// Execution Model: handler, queue
// =============================================================================

class handler {
    work_queue<std::function<void(void)>>* task_queue = nullptr;
    size_t worker_hint = 1;

    template <typename Submitter>
    void run_chunks(size_t total_work_items, Submitter submitter){
        constexpr size_t chunks_per_worker = 4U;
        if(total_work_items == 0){
            return;
        }

        const size_t desired_chunks = std::min(total_work_items, std::max<size_t>(worker_hint * chunks_per_worker, 1U));
        if((task_queue == nullptr) || (desired_chunks <= 1U)){
            submitter(0U, total_work_items);
            return;
        }

        std::mutex completion_mutex;
        std::condition_variable completion_cv;
        size_t completed_chunks = 0;
        std::mutex exception_mutex;
        std::exception_ptr captured_exception = nullptr;

        for(size_t chunk = 0; chunk < desired_chunks; ++chunk){
            const size_t begin = (total_work_items * chunk) / desired_chunks;
            const size_t end = (total_work_items * (chunk + 1U)) / desired_chunks;
            if(begin >= end){
                std::lock_guard<std::mutex> lock(completion_mutex);
                ++completed_chunks;
                continue;
            }

            task_queue->submit_task([&, begin, end](){
                try{
                    submitter(begin, end);
                }catch(...){
                    std::lock_guard<std::mutex> lock(exception_mutex);
                    if(!captured_exception){
                        captured_exception = std::current_exception();
                    }
                }
                std::lock_guard<std::mutex> lock(completion_mutex);
                ++completed_chunks;
                completion_cv.notify_one();
            });
        }

        std::unique_lock<std::mutex> lock(completion_mutex);
        completion_cv.wait(lock, [&](){ return completed_chunks == desired_chunks; });
        lock.unlock();

        if(captured_exception){
            std::rethrow_exception(captured_exception);
        }
    }

public:
    handler() = default;

    handler(work_queue<std::function<void(void)>>* q, size_t n_workers)
        : task_queue(q), worker_hint(std::max<size_t>(n_workers, 1U)) {}

    // Factory for creating accessors from buffers within a command group
    template <typename T, int Dims, access::mode Mode, access::target Target>
    void require(const accessor<T, Dims, Mode, Target>& acc) {
        // No-op in synchronous CPU fallback
    }

    // --- PARALLEL FOR IMPLEMENTATIONS ---

    // 1D
    template <typename KernelName = void, typename Func>
    void parallel_for(range<1> r, Func kernel) {
        this->run_chunks(r[0], [&](size_t begin, size_t end){
            for (size_t i = begin; i < end; ++i) {
                if constexpr (std::is_invocable_v<Func, item<1>>) {
                    kernel(item<1>(r, id<1>(i)));
                } else {
                    kernel(id<1>(i));
                }
            }
        });
    }

    // 2D
    template <typename KernelName = void, typename Func>
    void parallel_for(range<2> r, Func kernel) {
        const size_t total = r[0] * r[1];
        this->run_chunks(total, [&](size_t begin, size_t end){
            for (size_t linear = begin; linear < end; ++linear) {
                const size_t i = linear / r[1];
                const size_t j = linear % r[1];
                id<2> idx(i, j);
                if constexpr (std::is_invocable_v<Func, item<2>>) {
                    kernel(item<2>(r, idx));
                } else {
                    kernel(idx);
                }
            }
        });
    }

    // 3D
    template <typename KernelName = void, typename Func>
    void parallel_for(range<3> r, Func kernel) {
        const size_t row_size = r[2];
        const size_t plane_size = r[1] * r[2];
        const size_t total = r[0] * plane_size;
        this->run_chunks(total, [&](size_t begin, size_t end){
            for (size_t linear = begin; linear < end; ++linear) {
                const size_t i = linear / plane_size;
                const size_t rem = linear % plane_size;
                const size_t j = rem / row_size;
                const size_t k = rem % row_size;
                id<3> idx(i, j, k);
                if constexpr (std::is_invocable_v<Func, item<3>>) {
                    kernel(item<3>(r, idx));
                } else {
                    kernel(idx);
                }
            }
        });
    }
};

class queue {
    std::function<void(exception_list)> async_handler;
    size_t worker_count = 1;
    std::shared_ptr<work_queue<std::function<void(void)>>> task_queue;
    device dev_instance;

    void initialize_task_queue(){
        constexpr unsigned int fallback_worker_count = 2U;
        const unsigned int hw = std::thread::hardware_concurrency();
        this->worker_count = (hw == 0U) ? fallback_worker_count : static_cast<size_t>(hw);
        this->task_queue = std::make_shared<work_queue<std::function<void(void)>>>(static_cast<unsigned int>(this->worker_count));
    }

public:
    // Simple constructor
    queue() : dev_instance() { this->initialize_task_queue(); } 
    queue(default_selector_t) : dev_instance() { this->initialize_task_queue(); }
    template <class AsyncHandler>
    queue(default_selector_t, AsyncHandler h) : async_handler(h), dev_instance() { this->initialize_task_queue(); }

    // Submit a command group function (CGF)
    template <typename T>
    void submit(T cgf) {
        handler h(this->task_queue.get(), this->worker_count);
        cgf(h);
    }

    void wait() {
        // No-op: execution is already done
    }
    void wait_and_throw() {
        // No-op: execution is already done and no async backend exists.
    }

    template <typename Func>
    void parallel_for(range<1> r, Func kernel){
        handler h(this->task_queue.get(), this->worker_count);
        h.parallel_for(r, kernel);
    }
    template <typename Func>
    void parallel_for(range<2> r, Func kernel){
        handler h(this->task_queue.get(), this->worker_count);
        h.parallel_for(r, kernel);
    }
    template <typename Func>
    void parallel_for(range<3> r, Func kernel){
        handler h(this->task_queue.get(), this->worker_count);
        h.parallel_for(r, kernel);
    }

    // Other helpers
    device get_device() const {
        return dev_instance;
    }

};

template <class T>
T* malloc_shared(size_t count, queue&){
    return new T[count];
}

template <class T>
inline void free(T *ptr, queue&){
    delete[] ptr;
}

inline bool isfinite(float v){ return std::isfinite(v); }
inline bool isfinite(double v){ return std::isfinite(v); }
inline double floor(double v){ return std::floor(v); }
inline double sqrt(double v){ return std::sqrt(v); }
inline double exp(double v){ return std::exp(v); }


// =============================================================================
// Image Sampling Support (fallback implementation)
// =============================================================================

// Coordinate normalization mode for image samplers.
enum class coordinate_normalization_mode {
    unnormalized,  // Coordinates are in pixel indices (0 to N-1)
    normalized     // Coordinates are normalized to [0, 1]
};

// Addressing mode for out-of-bounds access.
enum class addressing_mode {
    none,           // Undefined behavior for out-of-bounds
    clamp_to_edge,  // Clamp to valid range
    clamp,          // Same as clamp_to_edge
    repeat,         // Wrap around
    mirrored_repeat // Mirror at edges
};

// Filtering mode for interpolation.
enum class filtering_mode {
    nearest,  // Nearest-neighbor sampling
    linear    // Linear interpolation
};

// Image sampler configuration.
// In the SYCL 2020 standard, image_sampler is a class that configures how images are sampled.
struct image_sampler {
    coordinate_normalization_mode coord_mode = coordinate_normalization_mode::unnormalized;
    addressing_mode addr_mode = addressing_mode::clamp_to_edge;
    filtering_mode filter_mode = filtering_mode::linear;

    image_sampler() = default;
    image_sampler(coordinate_normalization_mode cm, addressing_mode am, filtering_mode fm)
        : coord_mode(cm), addr_mode(am), filter_mode(fm) {}
};

// 4-component float vector type for image sampling results.
struct float4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    float4() = default;
    float4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// Sampled image class for 3D images.
// This is a fallback CPU implementation that provides trilinear interpolation.
// In real SYCL, hardware-accelerated texture sampling would be used.
template <typename DataT, int Dims>
class sampled_image {
    static_assert(Dims == 3, "Only 3D sampled images are supported in this fallback");
    
    const DataT* data_ptr;
    size_t width, height, depth;
    size_t channels;
    image_sampler sampler;

public:
    sampled_image(const DataT* data, size_t w, size_t h, size_t d, size_t ch, const image_sampler& s)
        : data_ptr(data), width(w), height(h), depth(d), channels(ch), sampler(s) {}

    // Read/sample from the image at given coordinates.
    // Coordinates are in the format (x, y, z) where each can be fractional.
    float4 read(double x, double y, double z) const {
        // Handle coordinate normalization
        if(sampler.coord_mode == coordinate_normalization_mode::normalized){
            x *= static_cast<double>(width);
            y *= static_cast<double>(height);
            z *= static_cast<double>(depth);
        }

        // Handle addressing mode for out-of-bounds coordinates
        auto clamp_coord = [](double c, size_t max_val) -> double {
            if(c < 0.0) return 0.0;
            if(c >= static_cast<double>(max_val)) return static_cast<double>(max_val) - 1.0;
            return c;
        };

        auto apply_addressing = [&](double c, size_t max_val) -> double {
            switch(sampler.addr_mode){
                case addressing_mode::clamp_to_edge:
                case addressing_mode::clamp:
                    return clamp_coord(c, max_val);
                case addressing_mode::repeat:
                    c = std::fmod(c, static_cast<double>(max_val));
                    if(c < 0) c += static_cast<double>(max_val);
                    return c;
                case addressing_mode::mirrored_repeat: {
                    double period = static_cast<double>(max_val);
                    c = std::fmod(c, 2.0 * period);
                    if(c < 0) c += 2.0 * period;
                    if(c >= period) c = 2.0 * period - c;
                    return clamp_coord(c, max_val);
                }
                default:
                    return c;
            }
        };

        x = apply_addressing(x, width);
        y = apply_addressing(y, height);
        z = apply_addressing(z, depth);

        if(sampler.filter_mode == filtering_mode::nearest){
            // Nearest-neighbor sampling
            int64_t ix = static_cast<int64_t>(std::round(x));
            int64_t iy = static_cast<int64_t>(std::round(y));
            int64_t iz = static_cast<int64_t>(std::round(z));
            ix = std::max<int64_t>(0, std::min<int64_t>(ix, static_cast<int64_t>(width) - 1));
            iy = std::max<int64_t>(0, std::min<int64_t>(iy, static_cast<int64_t>(height) - 1));
            iz = std::max<int64_t>(0, std::min<int64_t>(iz, static_cast<int64_t>(depth) - 1));
            
            float4 result;
            size_t idx = static_cast<size_t>(((iz * static_cast<int64_t>(height) + iy) * static_cast<int64_t>(width) + ix) * static_cast<int64_t>(channels));
            result.x = (channels > 0) ? static_cast<float>(data_ptr[idx + 0]) : 0.0f;
            result.y = (channels > 1) ? static_cast<float>(data_ptr[idx + 1]) : 0.0f;
            result.z = (channels > 2) ? static_cast<float>(data_ptr[idx + 2]) : 0.0f;
            result.w = (channels > 3) ? static_cast<float>(data_ptr[idx + 3]) : 0.0f;
            return result;
        }
        
        // Linear (trilinear) interpolation
        int64_t x0 = static_cast<int64_t>(std::floor(x));
        int64_t y0 = static_cast<int64_t>(std::floor(y));
        int64_t z0 = static_cast<int64_t>(std::floor(z));
        int64_t x1 = x0 + 1;
        int64_t y1 = y0 + 1;
        int64_t z1 = z0 + 1;

        double tx = x - static_cast<double>(x0);
        double ty = y - static_cast<double>(y0);
        double tz = z - static_cast<double>(z0);

        // Clamp indices to valid range
        auto clamp_idx = [](int64_t i, size_t max_val) -> size_t {
            if(i < 0) return 0;
            if(i >= static_cast<int64_t>(max_val)) return max_val - 1;
            return static_cast<size_t>(i);
        };

        auto sample_at = [&](int64_t ix, int64_t iy, int64_t iz, int64_t ch) -> float {
            size_t cix = clamp_idx(ix, width);
            size_t ciy = clamp_idx(iy, height);
            size_t ciz = clamp_idx(iz, depth);
            size_t idx = ((ciz * height + ciy) * width + cix) * channels + static_cast<size_t>(ch);
            return static_cast<float>(data_ptr[idx]);
        };

        float4 result;
        for(int64_t ch = 0; ch < static_cast<int64_t>(std::min<size_t>(channels, 4)); ++ch){
            // Sample the 8 corner voxels
            float c000 = sample_at(x0, y0, z0, ch);
            float c100 = sample_at(x1, y0, z0, ch);
            float c010 = sample_at(x0, y1, z0, ch);
            float c110 = sample_at(x1, y1, z0, ch);
            float c001 = sample_at(x0, y0, z1, ch);
            float c101 = sample_at(x1, y0, z1, ch);
            float c011 = sample_at(x0, y1, z1, ch);
            float c111 = sample_at(x1, y1, z1, ch);

            // Trilinear interpolation
            float c00 = c000 * (1.0f - static_cast<float>(tx)) + c100 * static_cast<float>(tx);
            float c10 = c010 * (1.0f - static_cast<float>(tx)) + c110 * static_cast<float>(tx);
            float c01 = c001 * (1.0f - static_cast<float>(tx)) + c101 * static_cast<float>(tx);
            float c11 = c011 * (1.0f - static_cast<float>(tx)) + c111 * static_cast<float>(tx);
            float c0 = c00 * (1.0f - static_cast<float>(ty)) + c10 * static_cast<float>(ty);
            float c1 = c01 * (1.0f - static_cast<float>(ty)) + c11 * static_cast<float>(ty);
            float val = c0 * (1.0f - static_cast<float>(tz)) + c1 * static_cast<float>(tz);

            switch(ch){
                case 0: result.x = val; break;
                case 1: result.y = val; break;
                case 2: result.z = val; break;
                case 3: result.w = val; break;
            }
        }
        return result;
    }
};

} // namespace sycl

#endif // SYCL_FALLBACK_HPP


//// =============================================================================
////  Example usage
//// =============================================================================
//#include <iostream>
//#include "SYCL.hpp"
//
//int main() {
//    constexpr size_t N = 16;
//    std::vector<int> v(N, 1);
//    
//    // Create queue
//    sycl::queue q;
//
//    // Create buffer wrapping host memory
//    sycl::buffer<int, 1> buf(v.data(), sycl::range<1>(N));
//
//    q.submit([&](sycl::handler& h) {
//        // Create accessor
//        sycl::accessor acc(buf, h);
//
//        // Run kernel
//        h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> i) {
//            acc[i] *= 2; // Should become 2
//        });
//    });
//
//    // Verification
//    for(int i=0; i<N; ++i) {
//        std::cout << v[i] << " "; // Should print 2 2 2 ...
//    }
//    std::cout << "\nSuccess.\n";
//    return 0;
//}
