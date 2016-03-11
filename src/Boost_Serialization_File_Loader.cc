//Boost_Serialization_File_Loader.cc - A part of DICOMautomaton 2016. Written by hal clark.
//
// This program loads data from files which have been serialized in Protobuf format.
//

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>

#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <boost/algorithm/string.hpp> //For boost:iequals().

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <boost/serialization/nvp.hpp>

//For plain-text archives.
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

//For XML archives.
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>

//For binary archives.
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <pqxx/pqxx>          //PostgreSQL C++ interface.

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorMathIOBoostSerialization.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesIOBoostSerialization.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.
#include "YgorDICOMTools.h"   //Needed for Is_File_A_DICOM_File(...);

#include "Structs.h"
#include "StructsIOBoostSerialization.h"

#include "Imebra_Shim.h"      //Wrapper for Imebra library. Black-boxed to speed up compilation.


bool Load_From_Boost_Serialization_Files( Drover &DICOM_data,
                                          std::map<std::string,std::string> & /* InvocationMetadata */,
                                          std::string &FilenameLex,
                                          std::list<boost::filesystem::path> &Filenames ){





    /*
    //////////////////////////////////////////////////////////////

    {
        vec3<double> A(1.0, 2.0, 3.0);
        std::ofstream ofs("/tmp/serial_vec3", std::ios::trunc);
        boost::archive::xml_oarchive ar(ofs);
        ar & boost::serialization::make_nvp("vec3double", A);
    }
    {
        vec3<double> A(100.0, 200.0, 300.0);
        std::ifstream ifs("/tmp/serial_vec3");
        try{
            boost::archive::xml_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("not used", A);
            FUNCINFO("Deserialized vec3 to " << A);
        }catch(const std::exception &){
            FUNCINFO("Unable to deserialize vec3 file. It is not valid");
        }
    }


    {
        line<double> A;
        std::ofstream ofs("/tmp/serial_line", std::ios::trunc);
        boost::archive::xml_oarchive ar(ofs);
        ar & boost::serialization::make_nvp("linedouble", A);
    }
    {
        line<double> A;
        std::ifstream ifs("/tmp/serial_line");
        try{
            boost::archive::xml_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("not used", A);
            FUNCINFO("Deserialized line to " << A.R_0 << "  " << A.U_0);
        }catch(const std::exception &){
            FUNCINFO("Unable to deserialize line file. It is not valid");
        }

    }


    {
        line_segment<double> A;
        A.t_0 = 2.3;
        A.t_1 = 3.2;
        std::ofstream ofs("/tmp/serial_line_segment", std::ios::trunc);
        boost::archive::xml_oarchive ar(ofs);
        ar & boost::serialization::make_nvp("line_segmentdouble", A);
    }
    {
        line_segment<double> A;
        std::ifstream ifs("/tmp/serial_line_segment");
        try{
            boost::archive::xml_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("not used", A);
            FUNCINFO("Deserialized line_segment to " << A.R_0 << "  " << A.U_0 << "   " << A.t_0 << "   " << A.t_1);
        }catch(const std::exception &){
            FUNCINFO("Unable to deserialize line_segment file. It is not valid");
        }

    }



    {
        plane<double> A;
        std::ofstream ofs("/tmp/serial_plane", std::ios::trunc);
        boost::archive::xml_oarchive ar(ofs);
        ar & boost::serialization::make_nvp("planedouble", A);
    }
    {
        plane<double> A;
        std::ifstream ifs("/tmp/serial_plane");
        try{
            boost::archive::xml_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("not used", A);
            FUNCINFO("Deserialized plane to " << A.N_0 << "  " << A.R_0);
        }catch(const std::exception &){
            FUNCINFO("Unable to deserialize plane file. It is not valid");
        }

    }


    {
        contour_of_points<double> A;
        A.points.push_back( vec3<double>(1.0,2.0,3.0) );
        A.points.push_back( vec3<double>(4.0,5.0,6.0) );
        A.points.push_back( vec3<double>(7.0,8.0,9.0) );
        A.closed = true;
        A.metadata["keyA"] = "valueA";
        A.metadata["keyB"] = "valueB";

        std::ofstream ofs("/tmp/serial_contour_of_points", std::ios::trunc);
        boost::archive::xml_oarchive ar(ofs);
        ar & boost::serialization::make_nvp("contour_of_pointsdouble", A);
    }
    {
        contour_of_points<double> A;
        std::ifstream ifs("/tmp/serial_contour_of_points");
        try{
            boost::archive::xml_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("not used", A);
            FUNCINFO("Deserialized contour_of_points to " << A.points.size() << " points and " << A.metadata.size() << " metadata");
        }catch(const std::exception &){
            FUNCINFO("Unable to deserialize contour_of_points file. It is not valid");
        }

    }


    {
        samples_1D<double> A;
        A.push_back(0.0,0.0,1.0,1.0);
        A.push_back(1.0,0.0,2.0,1.0);
        A.push_back(2.0,0.0,3.0,1.0);
        A.metadata["keyA"] = "valueA";
        A.metadata["keyB"] = "valueB";
        std::ofstream ofs("/tmp/serial_samples_1D", std::ios::trunc);
        boost::archive::xml_oarchive ar(ofs);
        ar & boost::serialization::make_nvp("samples_1Ddouble", A);
    }
    {
        samples_1D<double> A;
        std::ifstream ifs("/tmp/serial_samples_1D");
        try{
            boost::archive::xml_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("not used", A);
            FUNCINFO("Deserialized samples_1D to " << A.samples.size() << " samples and " << A.metadata.size() << " metadata");
        }catch(const std::exception &){
            FUNCINFO("Unable to deserialize samples_1D file. It is not valid");
        }

    }




    {
        planar_image<float,double> A;
        A.init_buffer(20,40,5);
        A.init_spatial(0.5,1.5,2.5, vec3<double>(0.0,0.0,0.0), vec3<double>(5.0,5.0,5.0));
        A.init_orientation(vec3<double>(1.0,0.0,0.0), vec3<double>(0.0,1.0,0.0));
        A.fill_pixels(1.23f);

        std::ofstream ofs("/tmp/serial_planar_image", std::ios::trunc);
        boost::archive::xml_oarchive ar(ofs);
        ar & boost::serialization::make_nvp("planar_imagefloatdouble", A);
    }
    {
        planar_image<float,double> A;
        std::ifstream ifs("/tmp/serial_planar_image");
        try{
            boost::archive::xml_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("not used", A);
            FUNCINFO("Deserialized planar_image to " << A.rows << "x" << A.columns << " image with first pixel = " << A.value(0,0,0));
        }catch(const std::exception &){
            FUNCINFO("Unable to deserialize planar_image file. It is not valid");
        }

    }


    {
        planar_image_collection<float,double> A;

        planar_image<float,double> B;
        B.init_buffer(20,40,5);
        B.init_spatial(0.5,1.5,2.5, vec3<double>(0.0,0.0,0.0), vec3<double>(5.0,5.0,5.0));
        B.init_orientation(vec3<double>(1.0,0.0,0.0), vec3<double>(0.0,1.0,0.0));
        B.fill_pixels(1.23f);

        planar_image<float,double> C = B;

        A.images.push_back(B);
        A.images.push_back(C);

        std::ofstream ofs("/tmp/serial_planar_image_collection", std::ios::trunc);
        boost::archive::xml_oarchive ar(ofs);
        ar & boost::serialization::make_nvp("planar_image_collectionfloatdouble", A);
    }
    {
        planar_image_collection<float,double> A;
        std::ifstream ifs("/tmp/serial_planar_image_collection");
        try{
            boost::archive::xml_iarchive ar(ifs);
            ar & boost::serialization::make_nvp("not used", A);
            FUNCINFO("Deserialized planar_image_collection to " << A.images.size() << " images, the first of which"
                   << " has " << A.images.front().rows << "x" << A.images.front().columns << " RxC with first pixel = "
                   << A.images.front().value(0,0,0));
        }catch(const std::exception &){
            FUNCINFO("Unable to deserialize planar_image_collection file. It is not valid");
        }

    }

    ///////////////////////////////////////////////////////////////

    */





    //Cycle through the known, acceptable types trying to parse until one works.
    std::list<boost::filesystem::path> Filenames_Copy(Filenames);
    Filenames.clear();
    for(const auto &fn : Filenames_Copy){
        std::ifstream fi(fn.string(), std::ios::binary);
        if(!fi) continue;

/* Needed with boost.serialize?
        //Filter out zero-length archives.
        {
            fi.seekg(0,fi.end);
            const auto length = fi.tellg();
            if(length == 0){
                Filenames.emplace_back(fn);
                continue;
            }
            fi.seekg(0,fi.beg);
        }
*/

        {
            fi.seekg(0);

            Drover A;
            try{
                boost::archive::xml_iarchive ar(fi);
                ar & boost::serialization::make_nvp("not used", A);

                FUNCINFO("Was a valid Drover class in XML format");
                continue;
            }catch(const std::exception &){
                FUNCINFO("Not a valid Drover class in XML format. Continuing");
            }
        }

        {
            fi.seekg(0);

            Drover A;
            try{
                boost::archive::binary_iarchive ar(fi);
                ar & boost::serialization::make_nvp("not used", A);

                FUNCINFO("Was a valid Drover class in binary format");

                //Replace the existing data. FOR TESTING ONLY
                DICOM_data = A;

                //Concatenate loaded data to the existing data.

                //  ... TODO ...

                continue;
            }catch(const std::exception &){
                FUNCINFO("Not a valid Drover class in binary format. Continuing");
            }
        }

        //Not a (known) file we can parse. Leave it as-is.
        Filenames.emplace_back(fn);
        continue;
    }

    return true;
}
