
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstdlib>            //Needed for exit() calls.
#include <cstdio>
#include <cstddef>
#include <exception>
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>    
#include <tuple>
#include <type_traits>
#include <utility>            //Needed for std::pair.
#include <vector>
#include <chrono>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <initializer_list>
#include <thread>
#include <random>
#include <cstdint>
#include <filesystem>


#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"

class Drover {
    int a;
    std::shared_ptr<Drover> b;
};

int main(){
    Drover contouring_imgs;
    std::shared_future<Drover> extracted_contours;
    //std::list< std::future<Drover> > old_futures;

    int64_t N_tests = 0;
    int64_t frame_count = 0;
    while(true){
        ++frame_count;

        if(0 < N_tests){
            std::cout << "Test complete. Exiting." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return 0;
        }

        const auto display_image_viewer = [ frame_count,
                                            &contouring_imgs,
                                            &extracted_contours,
                                            //&old_futures,
                                            &N_tests]() -> void {
            if(frame_count < 5){ 
                Drover local;
                auto l_work = [l_local = local]() mutable -> Drover {
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    return l_local;
                };
                //*****
                // Problematic codei with std::future, but not with std::shared_future:
                extracted_contours = std::async(std::launch::async, std::move(l_work));
                //
                //*****
                std::cout << "Launched async task" << std::endl;

            }else{
                const bool work_is_done = !extracted_contours.valid();
                if( !work_is_done ){
                    if(std::future_status::ready == extracted_contours.wait_for(std::chrono::microseconds(1))){
                        std::cout << "Async task is ready, extracting result." << std::endl;

                        try{
                            contouring_imgs = extracted_contours.get();
                        }catch(const std::exception &){};
                        extracted_contours = decltype(extracted_contours)();

                        std::cout << "Async result is extracted on frame " << frame_count << "." << std::endl;

                        ++N_tests;
                    }
                }
            }

            return;
        };
        try{
            display_image_viewer();
        }catch(const std::exception &e){
            std::cout << "Exception in display_image_viewer(): '" << e.what() << "'" << std::endl;
        }
    }

    return 1;
}
