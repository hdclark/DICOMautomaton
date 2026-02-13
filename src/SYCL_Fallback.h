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

namespace sycl {

namespace access {
    enum class mode { read, write, read_write, discard_write, discard_read_write, atomic };
    enum class target { global_buffer, constant_buffer, local, image, host_buffer };
    enum class placeholder { false_t, true_t };
}


// Forward declarations.

template <int Dims> struct range;
template <int Dims> struct id;
template <int Dims> struct item;

template <typename T, int Dims> class buffer;
template <typename T, int Dims, access::mode Mode, access::target Target> class accessor;

class handler;
class queue;
struct default_selector_t {};
inline constexpr default_selector_t default_selector_v{};
using exception_list = std::vector<std::exception_ptr>;


// =============================================================================
// 1. Basic Identifiers: id, range, item
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

// =============================================================================
// 2. Memory Model: buffer, accessor
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
    accessor(buffer<T, Dims>& buf, handler& h) 
        : ptr(buf.get_pointer()), r(buf.get_range()) {
        h.require(*this); // Register the dependency (even if it's a no-op)
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
// 3. Execution Model: handler, queue
// =============================================================================

class handler {
public:
    // Factory for creating accessors from buffers within a command group
    template <typename T, int Dims, access::mode Mode, access::target Target>
    void require(const accessor<T, Dims, Mode, Target>& acc) {
        // No-op in synchronous CPU fallback
    }

    // --- PARALLEL FOR IMPLEMENTATIONS ---

    // 1D
    template <typename KernelName = void, typename Func>
    void parallel_for(range<1> r, Func kernel) {
        for (size_t i = 0; i < r[0]; ++i) {
            if constexpr (std::is_invocable_v<Func, item<1>>) {
                kernel(item<1>(r, id<1>(i)));
            } else {
                kernel(id<1>(i));
            }
        }
    }

    // 2D
    template <typename KernelName = void, typename Func>
    void parallel_for(range<2> r, Func kernel) {
        for (size_t i = 0; i < r[0]; ++i) {
            for (size_t j = 0; j < r[1]; ++j) {
                id<2> idx(i, j);
                if constexpr (std::is_invocable_v<Func, item<2>>) {
                    kernel(item<2>(r, idx));
                } else {
                    kernel(idx);
                }
            }
        }
    }

    // 3D
    template <typename KernelName = void, typename Func>
    void parallel_for(range<3> r, Func kernel) {
        for (size_t i = 0; i < r[0]; ++i) {
            for (size_t j = 0; j < r[1]; ++j) {
                for (size_t k = 0; k < r[2]; ++k) {
                    id<3> idx(i, j, k);
                    if constexpr (std::is_invocable_v<Func, item<3>>) {
                        kernel(item<3>(r, idx));
                    } else {
                        kernel(idx);
                    }
                }
            }
        }
    }
};

class queue {
    std::function<void(exception_list)> async_handler;
public:
    // Simple constructor
    queue() {} 
    queue(default_selector_t) {}
    template <class Handler>
    queue(default_selector_t, Handler h) : async_handler(h) {}

    // Submit a command group function (CGF)
    template <typename T>
    void submit(T cgf) {
        handler h;
        // Execute the command group immediately (synchronous fallback)
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
        handler h;
        h.parallel_for(r, kernel);
    }
    template <typename Func>
    void parallel_for(range<2> r, Func kernel){
        handler h;
        h.parallel_for(r, kernel);
    }
    template <typename Func>
    void parallel_for(range<3> r, Func kernel){
        handler h;
        h.parallel_for(r, kernel);
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

} // namespace sycl

#endif // SYCL_FALLBACK_HPP


//// =============================================================================
//// 4. Example usage
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
