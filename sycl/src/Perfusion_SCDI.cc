//Perfusion_Model.cc -- A part of DICOMautomaton 2020. Written by hal clark, ...
//
// This file is meant to contain an implementation of the SCDI blood perfusion kinetic model.

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>    
#include <vector>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.

#include <boost/filesystem.hpp>

#include <CL/sycl.hpp>

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Perfusion_SCDI.h"


static
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


void
Launch_SCDI( samples_1D<double> &AIF,
             samples_1D<double> &VIF, 
             std::vector<samples_1D<double>> &C ){

    // The model should be fitted here. Keep in mind that the AIF, VIF, and all time courses in C are irregularly
    // sampled, so they might need to be re-sampled.

    // ...



    // The following is a simple example of calling the SYCL GPU-accelerated function above.
    std::vector<float> lhs = {  1.0f, -2.0f,  0.0f, -2.5f,  10.0f };
    std::vector<float> rhs = { -1.0f,  2.0f, -0.0f,  2.5f, -10.0f };

    cl::sycl::queue q;
    auto dst = vec_add(q, lhs, rhs);

    float sum = 0.0f;
    for(const auto x : dst) sum += x;

    if( (sum < -1E-4) || (1E-4 < sum) ){
        FUNCERR("Sum = " << sum << " (should be 0.0)");
    }else{
        FUNCINFO("SYCL function ran successfully.");
    }

    return;
};

