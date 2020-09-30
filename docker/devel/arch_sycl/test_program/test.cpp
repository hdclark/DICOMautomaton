// Adapted SYCL test program to verify that a SYCL compiler is available and works correctly.

#include <iostream>

#include <CL/sycl.hpp>

std::vector<float>
vec_add(cl::sycl::queue &q, const std::vector<float> &lhs, const std::vector<float> &rhs){
    const auto N = lhs.size();
    std::vector<float> dst(N); // Destination; buffer for the result.

    if(N != rhs.size()){
        throw std::runtime_error("This routine only supports same-size buffer addition. Cannot continue.");
    }
    if(N != dst.size()){
        throw std::runtime_error("This routine only supports same-size buffer outputs. Cannot continue.");
    }
  
    cl::sycl::range<1> work_items{ N };

    {
        cl::sycl::buffer<float> buff_lhs( lhs.data(), N );
        cl::sycl::buffer<float> buff_rhs( rhs.data(), N );
        cl::sycl::buffer<float> buff_dst( dst.data(), N );

        q.submit([&](cl::sycl::handler& cgh){
            auto access_lhs = buff_lhs.get_access< cl::sycl::access::mode::read  >(cgh);
            auto access_rhs = buff_rhs.get_access< cl::sycl::access::mode::read  >(cgh);
            auto access_dst = buff_dst.get_access< cl::sycl::access::mode::write >(cgh);

            cgh.parallel_for<class vector_add>(work_items,
                [=](cl::sycl::id<1> tid){
                    access_dst[tid] = access_lhs[tid] + access_rhs[tid];
                }
            );
        });
    }
    return dst;
}

int main(int, char**){
    std::vector<float> lhs = {  1.0f, -2.0f,  0.0f, -2.5f,  10.0f };
    std::vector<float> rhs = { -1.0f,  2.0f, -0.0f,  2.5f, -10.0f };

    cl::sycl::queue q;
    auto dst = vec_add(q, lhs, rhs);

    float sum = 0.0f;
    for(const auto x : dst) sum += x;

    if( (sum < -1E-4) || (1E-4 < sum) ){
        std::cout << "Sum = " << sum << " (should be 0.0)" << std::endl;
        return 1;
    }else{
        std::cout << "Program ran successfully." << std::endl;
    }

    return 0;
}

