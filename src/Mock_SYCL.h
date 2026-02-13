// Mock_SYCL.h.

// This is a *mock*, minimal, CPU-only implementation of SYCL.hpp.
// It is meant to help compile and run SYCL code when the compiler or toolchain lacks support.
// Code compiled with this mock header will NOT have any runtime support.
// Based on the SYCL 2020 standard (but missing a lot of functionality!).

#ifndef MOCK_SYCL_HPP
#define MOCK_SYCL_HPP

#include <vector>
#include <array>
#include <functional>
#include <cstddef>
#include <algorithm>
#include <memory>
#include <iostream>

namespace sycl {

// =============================================================================
// 1. Basic Identifiers: id, range, item
// =============================================================================

template <int Dims>
struct range {
    std::array<size_t, Dims> dims;

    range(std::initializer_list<size_t> list) {
        std::copy(list.begin(), list.end(), dims.begin());
    }
    
    // Variadic constructor to allow range(x, y, z)
    template<typename... Args>
    range(Args... args) : dims{static_cast<size_t>(args)...} {}

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

    id() = default;
    
    template<typename... Args>
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

namespace access {
    enum class mode { read, write, read_write, discard_write, discard_read_write, atomic };
    enum class target { global_buffer, constant_buffer, local, image, host_buffer };
    enum class placeholder { false_t, true_t };
}

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
    accessor(buffer<T, Dims>& buf, const std::shared_ptr<void>&) 
        : ptr(buf.get_pointer()), r(buf.get_range()) {}

    // 1D Access
    T& operator[](id<1> index) const {
        return ptr[index[0]];
    }
    T& operator[](size_t index) const {
        return ptr[index];
    }

    // 2D Access
    T& operator[](id<2> index) const {
        // Flatten: y * width + x (assuming row-major matches standard C++)
        return ptr[index[0] * r[1] + index[1]];
    }

    // 3D Access
    T& operator[](id<3> index) const {
        // Flatten: z * (height * width) + y * width + x
        return ptr[index[0] * (r[1] * r[2]) + index[1] * r[2] + index[2]];
    }
};

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
public:
    // Simple constructor
    queue() {} 

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
};

} // namespace sycl

#endif // MOCK_SYCL_HPP


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
