
#include <limits>
#include <array>
#include <utility>
#include <iostream>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <fstream>

#include "YgorMath.h"
#include "YgorString.h"
#include "YgorFilesDirs.h"

#include "doctest/doctest.h"

#ifdef DCMA_USE_EIGEN
#else
    #error "Attempted to compile without Eigen support, which is required."
#endif
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/LU>

#include "MRI_IVIM_2.h"
using namespace MRI_IVIM_2;

TEST_CASE( "MRI_IVIM_2" ){
    //const auto nan = std::numeric_limits<double>::quiet_NaN();
    //const auto inf = std::numeric_limits<double>::infinity();
    const double pi = 3.141592653;

    const std::filesystem::path basedir{"MRI_IVIM_2"};
    std::vector<std::filesystem::path> test_files;
    for(auto const &f : std::filesystem::directory_iterator{basedir}){
        if(f.path().extension() == ".csv"){
            test_files.push_back(f.path());
        }
    }

    for(const auto &f : test_files){
        CAPTURE( f.string() );
        //std::cout << f << std::endl;

        // Parse the file. Sample test file:
        //
        //       # IVIM two-compartment model: S(b) = S0 * [ f*exp(-b*(D+D*)) + (1-f)*exp(-b*D) ]
        //       "# Parameters: S0=1.0, f=0.3, D=0.001 mm^2/s, D*=0.01 mm^2/s"
        //       "# Units: b [s/mm^2], S [a.u.]"
        //       b,S
        //       0,1.000000
        //       20,0.926895
        //       30,0.894989
        //       ...
        //       800,0.314575
        //       1000,0.257521
        auto lines = LoadFileToList(f.string());
        REQUIRE(lines.size() > 5);

        const auto desc = lines.front();
        lines.pop_front();
        CAPTURE( desc );

        const auto params_str = lines.front();
        lines.pop_front();
        const auto l_S0 = stringtoX<double>( GetAllRegex2(params_str, "S0=([^ ,]*)").at(0) );
        const auto l_f  = stringtoX<double>( GetAllRegex2(params_str, "f=([^ ,]*)").at(0) );
        const auto l_D  = stringtoX<double>( GetAllRegex2(params_str, "D=([^ ,]*)").at(0) );
        const auto l_Dp = stringtoX<double>( GetAllRegex2(params_str, "D[*]=([^ ,]*)").at(0) );


        std::vector<float> b_vals;
        std::vector<float> S_vals;
        int num_iters = 1000;
        for(const auto &l : lines){
            const auto p = GetAllRegex2(l, "([0-9.Ee+-]*),([0-9.Ee+-]*)");
            if(p.size() == 2UL){
                b_vals.emplace_back( stringtoX<double>( p.at(0) ) );
                S_vals.emplace_back( stringtoX<double>( p.at(1) ) );
            }
        }
        REQUIRE(b_vals.size() == S_vals.size());
        REQUIRE(b_vals.size() > 3UL);


        //std::array<double,3> out = GetBiExp(b_vals, S_vals, num_iters);
        const auto out  = GetBiExp(b_vals, S_vals, num_iters);
        const auto m_f  = out.at(0);
        const auto m_D  = out.at(1);
        const auto m_Dp = out.at(2);

        REQUIRE(l_f  == m_f);
        REQUIRE(l_D  == m_D);
        REQUIRE(l_Dp == m_Dp);

        //std::array<double, 3> GetKurtosisParams(const std::vector<float> &bvalues, const std::vector<float> &vals, int numIterations);
    }

    SUBCASE("blah"){
        REQUIRE(pi == pi);
    }
}

