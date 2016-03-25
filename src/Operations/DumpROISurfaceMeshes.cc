//DumpROISurfaceMeshes.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

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

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

//#include <CGAL/Simple_cartesian.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Scale_space_surface_reconstruction_3.h>
#include <CGAL/IO/read_off_points.h>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Linear.h"
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Cheby.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"

#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"

#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"

#include "DumpROISurfaceMeshes.h"


//CGAL quantities.

//typedef CGAL::Simple_cartesian<double> Kernel;
typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;

typedef CGAL::Scale_space_surface_reconstruction_3<Kernel> Reconstruction;

//typedef Reconstruction::Point Point;
typedef Kernel::Point_3 Point;



// function for writing the reconstruction output in the off format
static void dump_reconstruction(const Reconstruction &reconstruct, std::string name){
  std::ofstream output(name.c_str());
  output << "OFF " << reconstruct.number_of_points() << " "
         << reconstruct.number_of_triangles() << " 0\n";
  std::copy(reconstruct.points_begin(),
            reconstruct.points_end(),
            std::ostream_iterator<Point>(output,"\n"));
  for( Reconstruction::Triple_const_iterator it = reconstruct.surface_begin(); it != reconstruct.surface_end(); ++it )
      output << "3 " << *it << std::endl;

  return;
}




std::list<OperationArgDoc> OpArgDocDumpROISurfaceMeshes(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "OutDir";
    out.back().desc = "The directory in which to dump surface mesh files."
                      " It will be created if it doesn't exist.";
    out.back().default_val = "/tmp/";
    out.back().expected = true;
    out.back().examples = { "./", "../somedir/", "/path/to/some/destination" };

    return out;
}

Drover DumpROISurfaceMeshes(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //This operation generates surface meshes from contour volumes. Output is written to file(s) for viewing with an
    // external viewer (e.g., meshlab).
    // 

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto OutDir = OptArgs.getValueStr("OutDir").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    //-----------------------------------------------------------------------------------------------------------------
    //const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::grep);
    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    bool use_pca_normal_estimation = true;
    bool use_jet_normal_estimation = false;


    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    std::list<std::reference_wrapper<contour_collection<double>>> cc_all;
    for(auto & cc : DICOM_data.contour_data->ccs){
        auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
        cc_all.push_back( std::ref(*base_ptr) );
    }

    //Whitelist contours using the provided regex.
    auto cc_ROIs = cc_all;
    cc_ROIs.remove_if([=](std::reference_wrapper<contour_collection<double>> cc) -> bool {
                   const auto ROINameOpt = cc.get().contours.front().GetMetadataValueAs<std::string>("ROIName");
                   const auto ROIName = ROINameOpt.value();
                   return !(std::regex_match(ROIName,theregex));
    });


    //Gather all contours for each volume of interest.
    typedef std::tuple<std::string,std::string,std::string> key_t; //PatientID, ROIName, NormalizedROIName.
    typedef std::list<std::reference_wrapper< contour_of_points<double>>> val_t; 

    std::map<key_t,val_t> ROIs;
    if(DICOM_data.contour_data != nullptr){
        //for(auto & cc : DICOM_data.contour_data->ccs){
        //    for(auto & c : cc.contours){
        for(auto & cc_refw : cc_ROIs){
            for(auto & c : cc_refw.get().contours){
                key_t key = std::tie(c.metadata["PatientID"], c.metadata["ROIName"], c.metadata["NormalizedROIName"]);
                ROIs[key].push_back( std::ref(c) );
            }
        }
    }

    //For each volume of interest, generate a surface mesh.
    //
    // TODO: will this handle unconnected organs, e.g., left+right parotids, in the correct way?
    //
    for(auto & anROI : ROIs){
        const auto PatientID = std::get<0>(anROI.first);
        const auto ROIName   = std::get<1>(anROI.first);

        const auto OutFile = OutDir + "/ROI_Surface_Mesh_" + PatientID + "_" + ROIName + ".off";

        std::list<Point> points;
        for(auto & c : anROI.second){
            //const auto subdiv_c = c.get().Subdivide_Midway().Subdivide_Midway();
            const auto subdiv_c = c.get();


            //Assuming the contour is planar, determine the normal.
            const auto centroid = subdiv_c.Centroid();
            const auto pA = *(subdiv_c.points.begin());
            const auto pB = *(++subdiv_c.points.begin());
            const auto N = (pB-centroid).Cross(pA-centroid).unit();

            //Assume or determine some thickness for the contour. (Should be distance to nearest-neighbour or linear
            // voxel size of some sort).
            const double thickness = 3.0;

            //Lay down some points over the volume defined by the extruded contour 'disc'.
            for(double thick = -thickness * 0.5 ; thick <= thickness * 0.5; thick += thickness * 0.25 ){
                for(const auto &p : subdiv_c.points){
                    auto pp = p + N * thick;
                    points.push_back( Point(pp.x, pp.y, pp.z) );
                }
            }
        }

        std::cout << "done: " << points.size() << " points." << std::endl;
        // Construct the reconstruction with parameters for
        // the neighborhood squared radius estimation.
        Reconstruction reconstruct( 20, 1000 );

        // Add the points.
        reconstruct.insert( points.begin(), points.end() );

        // Advance the scale-space several steps.
        // This automatically estimates the scale-space.
        reconstruct.increase_scale( 2 );

        // Reconstruct the surface from the current scale-space.
        std::cout << "Neighborhood squared radius is " << reconstruct.neighborhood_squared_radius() << std::endl;

        reconstruct.reconstruct_surface();

        // Write the reconstruction.
        dump_reconstruction(reconstruct, OutFile);

        // Advancing the scale-space further and visually compare the reconstruction result
        reconstruct.increase_scale( 2 );

        // Reconstruct the surface from the current scale-space.
        std::cout << "Neighborhood squared radius is " << reconstruct.neighborhood_squared_radius() << std::endl;

        reconstruct.reconstruct_surface();
        // Write the reconstruction.
        //dump_reconstruction(reconstruct, OutFile);

        FUNCINFO("Wrote surface mesh file '" << OutFile << "'");

    }


/*
    Explicator X(FilenameLex);
    for(auto & NameCount : NameCounts){
        //Simply dump the suspected mapping.
        //std::cout << "PatientID='" << std::get<0>(NameCount.first) << "'\t"
        //          << "ROIName='" << std::get<1>(NameCount.first) << "'\t"
        //          << "NormalizedROIName='" << std::get<2>(NameCount.first) << "'\t"
        //          << "Contours='" << NameCount.second << "'"
        //          << std::endl;

        //Print out the best few guesses for each raw contour name.
        const auto ROIName = std::get<1>(NameCount.first);
        X(ROIName);
        std::unique_ptr<std::map<std::string,float>> res(std::move(X.Get_Last_Results()));
        std::vector<std::pair<std::string,float>> ordered_res(res->begin(), res->end());
        std::sort(ordered_res.begin(), ordered_res.end(), 
                  [](const std::pair<std::string,float> &L, const std::pair<std::string,float> &R) -> bool {
                      return L.second > R.second;
                  });
        if(ordered_res.size() != 1) for(size_t i = 0; i < ordered_res.size(); ++i){
            std::cout << ordered_res[i].first << " : " << ROIName; // << std::endl;
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
*/

    
    return std::move(DICOM_data);
}
