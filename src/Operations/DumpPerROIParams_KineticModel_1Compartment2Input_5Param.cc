//DumpPerROIParams_KineticModel_1Compartment2Input_5Param.cc - A part of DICOMautomaton 2016. Written by hal clark.

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
#include <regex>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

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
#include "../YgorImages_Functors/Processing/Liver_Pharmacokinetic_Model_5Param_Structs.h"
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

#include "../Common_Boost_Serialization.h"

#include "../KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "../KineticModel_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.h"

#include "../KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "../KineticModel_1Compartment2Input_5Param_Chebyshev_LevenbergMarquardt.h"
#include "../KineticModel_1Compartment2Input_5Param_Chebyshev_FreeformOptimization.h"


#include "DumpPerROIParams_KineticModel_1Compartment2Input_5Param.h"



std::list<OperationArgDoc> OpArgDocDumpPerROIParams_KineticModel_1Compartment2Input_5Param(void){
    std::list<OperationArgDoc> out;

    out.emplace_back();
    out.back().name = "ROILabelRegex";
    out.back().desc = "A regex matching ROI labels/names to consider. The default will match"
                      " all available ROIs. Be aware that input spaces are trimmed to a single space."
                      " If your ROI name has more than two sequential spaces, use regex to avoid them."
                      " All ROIs have to match the single regex, so use the 'or' token if needed."
                      " Regex is case insensitive and uses extended POSIX syntax.";
    out.back().default_val = ".*";
    out.back().expected = true;
    out.back().examples = { ".*", ".*body.*", "body", "Gross_Liver",
                            R"***(.*left.*parotid.*|.*right.*parotid.*|.*eyes.*)***",
                            R"***(left_parotid|right_parotid)***" };

    out.emplace_back();
    out.back().name = "Filename";
    out.back().desc = "A file into which the results should be dumped. If the filename is empty, the"
                      " results are dumped to the console only.";
    out.back().default_val = "";
    out.back().expected = true;
    out.back().examples = { "/tmp/results.txt", "/dev/null", "~/output.txt" };

    out.emplace_back();
    out.back().name = "Separator";
    out.back().desc = "The token(s) to place between adjacent columns of output. Note: because"
                      " whitespace is trimmed from user parameters, whitespace separators other than"
                      " the default are shortened to an empty string. So non-default whitespace are"
                      " not currently supported."; // I do not *currently* need this to be fixed...
    out.back().default_val = " ";
    out.back().expected = true;
    out.back().examples = { ",", ";", "_a_long_separator_" };

    return out;
}



Drover 
DumpPerROIParams_KineticModel_1Compartment2Input_5Param(Drover DICOM_data, 
                                                        OperationArgPkg OptArgs, 
                                                        std::map<std::string,std::string> /*InvocationMetadata*/, 
                                                        std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto Filename = OptArgs.getValueStr("Filename").value();
    const auto Separator = OptArgs.getValueStr("Separator").value();

    //-----------------------------------------------------------------------------------------------------------------
    const auto theregex = std::regex(ROILabelRegex, std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);


    //To keep the results sorted per-ROI (but in the same file) we have to defer output until all data is gathered.
    struct param_shtl {
        double k1A;
        double tauA;
        double k1V;
        double tauV;
        double k2;

        double RSS;
    };
    std::map< std::string, std::vector<param_shtl> > params; // key: ROI name.

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
    if(cc_ROIs.empty()){
        throw std::runtime_error("No contours selected/remaining. Cannot continue.");
    }


    //Figure out which image collection corresponds to each of the needed images or parameter maps.
    std::regex  k1A_regex(".*k1A.*",  std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    std::regex tauA_regex(".*tauA.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    std::regex  k1V_regex(".*k1V.*",  std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    std::regex tauV_regex(".*tauV.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
    std::regex   k2_regex(".*k2.*",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

    planar_image_collection<float,double> *imgcoll_k1A   = nullptr;
    planar_image_collection<float,double> *imgcoll_tauA  = nullptr;
    planar_image_collection<float,double> *imgcoll_k1V   = nullptr;
    planar_image_collection<float,double> *imgcoll_tauV  = nullptr;
    planar_image_collection<float,double> *imgcoll_k2    = nullptr;
    planar_image_collection<float,double> *imgcoll_ROI   = nullptr;

    for(auto & img_data_ptr : DICOM_data.image_data){
        auto *img_coll = &(img_data_ptr->imagecoll);

        auto common_metadata = img_coll->get_common_metadata({});
        const auto desc = common_metadata["Description"];
        if(desc.empty()) continue;
        if(img_coll->images.empty()) continue;

        if(false){
        }else if(std::regex_match(desc, k1A_regex  )){ imgcoll_k1A  = img_coll;
        }else if(std::regex_match(desc, tauA_regex )){ imgcoll_tauA = img_coll;
        }else if(std::regex_match(desc, k1V_regex  )){ imgcoll_k1V  = img_coll;
        }else if(std::regex_match(desc, tauV_regex )){ imgcoll_tauV = img_coll;
        }else if(std::regex_match(desc, k2_regex   )){ imgcoll_k2   = img_coll;
        }else{ 
            //We assume anything else is the ROI. Permits arbitrary pre-processing.
            FUNCWARN("Assuming ROI image_collection has description '" << desc << "'");
            imgcoll_ROI = img_coll;
        }
    }

    if( (DICOM_data.image_data.size() < 6)
      ||  (imgcoll_k1A == nullptr) 
      ||  (imgcoll_tauA == nullptr) 
      ||  (imgcoll_k1V == nullptr) 
      ||  (imgcoll_tauV == nullptr) 
      ||  (imgcoll_k2 == nullptr) 
      ||  (imgcoll_ROI == nullptr) ) throw std::runtime_error("Required image sets not located");

    //Assume that once we find the serialized model state (AIF and VIF) that we can assume the rest of the images will
    // have the same state. This SHOULD be the case for all sane situations, but there might be special cases where the
    // assumption fails.
    //
    // If we do make the assumption, it saves having to continually deserialize over and over. This will save a LOT of
    // time.
    enum {
        Have_No_Model,
        Have_1Compartment2Input_5Param_LinearInterp_Model,
        Have_1Compartment2Input_5Param_Chebyshev_Model
    } HaveModel;
    HaveModel = Have_No_Model;

    KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters model_params_linear;
    KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters model_params_cheby;

    //Now we iterate over one image set (whichever is arbitrary, but it helps if there is no temporal axis).
    // The procedure is a bit convoluted: we need to 'bootstrap' some knowledge about the voxel topology, so we choose
    // one image set to be representative of the others (this assumption is verified) and iterate over it's images.
    // For each image, we select the images that spatially overlap it from all image sets. We then take those and scan
    // over them, performing some computation. The bootstrapping part is the part that makes it a bit cumbersome. It
    // would probably be better to just zip a bunch of voxel iterators together and step through voxels one-by-one.
    // (But of course then we may lose the ability to combine voxels in groups/clusters or filter voxels out. I'm sure
    // there is a better way to do this. The current way is inelegant, but at least it works!)
    //
    auto all_k1A_images = imgcoll_k1A->get_all_images();
    while(!all_k1A_images.empty()){
        FUNCINFO("Images still to be processed: " << all_k1A_images.size());

        //Find the images which spatially overlap with this image.
        auto curr_k1A_img_it = all_k1A_images.front();
        auto selected_k1A_imgs  = GroupSpatiallyOverlappingImages(curr_k1A_img_it, std::ref(*imgcoll_k1A));
        auto selected_tauA_imgs = GroupSpatiallyOverlappingImages(curr_k1A_img_it, std::ref(*imgcoll_tauA));
        auto selected_k1V_imgs  = GroupSpatiallyOverlappingImages(curr_k1A_img_it, std::ref(*imgcoll_k1V));
        auto selected_tauV_imgs = GroupSpatiallyOverlappingImages(curr_k1A_img_it, std::ref(*imgcoll_tauV));
        auto selected_k2_imgs   = GroupSpatiallyOverlappingImages(curr_k1A_img_it, std::ref(*imgcoll_k2));
        auto selected_ROI_imgs  = GroupSpatiallyOverlappingImages(curr_k1A_img_it, std::ref(*imgcoll_ROI));

        if(selected_k1A_imgs.empty()){
            throw std::logic_error("No spatially-overlapping images found. There should be at least one"
                                   " image (the 'seed' image) which should match. Verify the spatial" 
                                   " overlap grouping routine.");
        }
        if(false){
        }else if( selected_tauA_imgs.empty() ){
            throw std::runtime_error("Missing spatially overlapping image in tauA map.");
        }else if( selected_k1V_imgs.empty() ){
            throw std::runtime_error("Missing spatially overlapping image in k1V map.");
        }else if( selected_tauV_imgs.empty() ){
            throw std::runtime_error("Missing spatially overlapping image in tauV map.");
        }else if( selected_k2_imgs.empty() ){
            throw std::runtime_error("Missing spatially overlapping image in k2 map.");
        }else if( selected_ROI_imgs.empty() ){
            throw std::runtime_error("Missing spatially overlapping image in ROI map.");
        }

        for(const auto & selected_imgs : { selected_k1A_imgs,  selected_tauA_imgs, selected_k1V_imgs,
                                           selected_tauV_imgs, selected_k2_imgs,   selected_ROI_imgs} ){
          auto rows     = curr_k1A_img_it->rows;
          auto columns  = curr_k1A_img_it->columns;
          auto channels = curr_k1A_img_it->channels;

          for(const auto &an_img_it : selected_imgs){
              if( (rows     != an_img_it->rows)
              ||  (columns  != an_img_it->columns)
              ||  (channels != an_img_it->channels) ){
                  throw std::domain_error("Images have differing number of rows, columns, or channels."
                                          " This is not currently supported -- though it could be if needed."
                                          " Are you sure you've got the correct data?");
              }
              // NOTE: We assume the first image in the selected_images set is representative of the following
              //       images. We assume they all share identical row and column units, spatial extent, planar
              //       orientation, and (possibly) that row and column indices for one image are spatially
              //       equal to all other images. Breaking the last assumption would require an expensive 
              //       position_space-to-row_and_column_index lookup for each voxel.
          }
        }

        for(auto &an_img_it : selected_k1A_imgs){
             all_k1A_images.remove(an_img_it); //std::list::remove() erases all elements equal to input value.
        }
        planar_image<float,double> &img = std::ref(*selected_k1A_imgs.front());
        //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
        const auto row_unit   = img.row_unit;
        const auto col_unit   = img.col_unit;
        const auto ortho_unit = row_unit.Cross( col_unit ).unit();
    
        //Look for serialized model_params. Deserialize them. We basically are finding AIF and VIF only here.
        //
        // Note: we have to do this first, before loading voxel-specific data (e.g., k1A) because the
        // model_state is stripped of individual-voxel-specific data. Deserialization will overwrite 
        // individual-voxel-specific data with NaNs.
        //
        if(HaveModel == Have_No_Model){
            for(auto & selected_imgs : { selected_k1A_imgs,  selected_tauA_imgs, 
                                         selected_k1V_imgs,  selected_tauV_imgs, 
                                         selected_k2_imgs,   selected_ROI_imgs } )
            for(auto & img_it : selected_imgs){
                if(auto m_str = img_it->GetMetadataValueAs<std::string>("ModelState")){
                    if(HaveModel == Have_No_Model){
                        if(Deserialize(m_str.value(),model_params_cheby)){
                            HaveModel = Have_1Compartment2Input_5Param_Chebyshev_Model;
                        }else if(Deserialize(m_str.value(),model_params_linear)){
                            HaveModel = Have_1Compartment2Input_5Param_LinearInterp_Model;
                        }else{
                            throw std::runtime_error("Unable to deserialize model parameters. Is the record damaged?");
                        }
                        break;
                    }
                }
            }
        }
        if(HaveModel == Have_No_Model){
            throw std::logic_error("We should have a valid model here, but do not.");
        }
                        
        //Loop over the ccsl, rois, rows, columns, channels, and finally any selected images (if applicable).
        //for(const auto &roi : rois){
        for(auto &ccs : cc_ROIs){
            for(auto roi_it = ccs.get().contours.begin(); roi_it != ccs.get().contours.end(); ++roi_it){
                if(roi_it->points.empty()) continue;
                if(! img.encompasses_contour_of_points(*roi_it)) continue;
    
                const auto ROIName =  roi_it->GetMetadataValueAs<std::string>("ROIName");
                if(!ROIName){
                    throw std::runtime_error("Missing necessary tags for reporting analysis results. Cannot continue");
                }
                
        /*
                //Construct a bounding box to reduce computational demand of checking every voxel.
                auto BBox = roi_it->Bounding_Box_Along(row_unit, 1.0);
                auto BBoxBestFitPlane = BBox.Least_Squares_Best_Fit_Plane(vec3<double>(0.0,0.0,1.0));
                auto BBoxProjectedContour = BBox.Project_Onto_Plane_Orthogonally(BBoxBestFitPlane);
                const bool BBoxAlreadyProjected = true;
        */
        
                //Prepare a contour for fast is-point-within-the-polygon checking.
                auto BestFitPlane = roi_it->Least_Squares_Best_Fit_Plane(ortho_unit);
                auto ProjectedContour = roi_it->Project_Onto_Plane_Orthogonally(BestFitPlane);
                const bool AlreadyProjected = true;
        
                for(auto row = 0; row < img.rows; ++row){
                    for(auto col = 0; col < img.columns; ++col){
                        //Figure out the spatial location of the present voxel.
                        const auto point = img.position(row,col);
        
        /*
                        //Check if within the bounding box. It will generally be cheaper than the full contour (4 points vs. ? points).
                        auto BBoxProjectedPoint = BBoxBestFitPlane.Project_Onto_Plane_Orthogonally(point);
                        if(!BBoxProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BBoxBestFitPlane,
                                                                                            BBoxProjectedPoint,
                                                                                            BBoxAlreadyProjected)) continue;
        */
        
                        //Perform a more detailed check to see if we are in the ROI.
                        auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(point);
                        if(ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                       ProjectedPoint,
                                                                                       AlreadyProjected)){
                            for(auto chan = 0; chan < img.channels; ++chan){

                                model_params_cheby.k1A  = std::numeric_limits<double>::quiet_NaN();
                                model_params_cheby.tauA = std::numeric_limits<double>::quiet_NaN();
                                model_params_cheby.k1V  = std::numeric_limits<double>::quiet_NaN();
                                model_params_cheby.tauV = std::numeric_limits<double>::quiet_NaN();
                                model_params_cheby.k2   = std::numeric_limits<double>::quiet_NaN();
                                model_params_cheby.RSS  = std::numeric_limits<double>::quiet_NaN();

                                model_params_linear.k1A  = std::numeric_limits<double>::quiet_NaN();
                                model_params_linear.tauA = std::numeric_limits<double>::quiet_NaN();
                                model_params_linear.k1V  = std::numeric_limits<double>::quiet_NaN();
                                model_params_linear.tauV = std::numeric_limits<double>::quiet_NaN();
                                model_params_linear.k2   = std::numeric_limits<double>::quiet_NaN();
                                model_params_linear.RSS  = std::numeric_limits<double>::quiet_NaN();

                                //Scan through the various image_collection images to locate 
                                // individual-voxel-specific data needed to evaluate the model.
                                samples_1D<double> ROI_time_course;
                                for(auto & selected_imgs : { selected_k1A_imgs,  selected_tauA_imgs, 
                                                             selected_k1V_imgs,  selected_tauV_imgs, 
                                                             selected_k2_imgs,   selected_ROI_imgs } )
                                for(auto & img_it : selected_imgs){
                                    const auto pxl_val = static_cast<double>(img_it->value(row, col, chan));

                                    //Now have pixel value. Figure out what to do with it.
                                    if(auto desc = img_it->GetMetadataValueAs<std::string>("Description")){

                                        if(false){
                                        }else if(std::regex_match(desc.value(), k1A_regex)){
                                            model_params_linear.k1A = pxl_val;
                                            model_params_cheby.k1A  = pxl_val;
                                        }else if(std::regex_match(desc.value(), tauA_regex)){
                                            model_params_linear.tauA = pxl_val;
                                            model_params_cheby.tauA  = pxl_val;
                                        }else if(std::regex_match(desc.value(), k1V_regex)){
                                            model_params_linear.k1V = pxl_val;
                                            model_params_cheby.k1V  = pxl_val;
                                        }else if(std::regex_match(desc.value(), tauV_regex)){
                                            model_params_linear.tauV = pxl_val;
                                            model_params_cheby.tauV  = pxl_val;
                                        }else if(std::regex_match(desc.value(), k2_regex)){
                                            model_params_linear.k2 = pxl_val;
                                            model_params_cheby.k2  = pxl_val;
                    
                                        }else if(auto dt = img_it->GetMetadataValueAs<double>("dt")){
                                            ROI_time_course.push_back(dt.value(), 0.0, pxl_val, 0.0);
                                        }
                                    }
                                }

                                // --------------- Compute derivative quantities, if desired -----------------

                                double RSS = 0.0;
                                try{
                                    for(auto & ROIsample : ROI_time_course.samples){
                                        const double t = ROIsample[0];
                                        const double f = ROIsample[2];
                                        if(HaveModel == Have_1Compartment2Input_5Param_LinearInterp_Model){
                                            KineticModel_1Compartment2Input_5Param_LinearInterp_Results eval_res;
                                            Evaluate_Model(model_params_linear,t,eval_res);
                                            RSS += std::pow(eval_res.I - f, 2.0);
                                        }else if(HaveModel == Have_1Compartment2Input_5Param_Chebyshev_Model){
                                            KineticModel_1Compartment2Input_5Param_Chebyshev_Results eval_res;
                                            Evaluate_Model(model_params_cheby,t,eval_res);
                                            RSS += std::pow(eval_res.I - f, 2.0);
                                        }
                                    }
                                }catch(const std::exception &e){
                                    RSS = std::numeric_limits<double>::quiet_NaN();
                                }


                                // --------------- Append the data into the user_data struct -----------------
                                const auto RName = ROIName.value();

                                params[RName].emplace_back();
                                if(HaveModel == Have_1Compartment2Input_5Param_LinearInterp_Model){
                                    params[RName].back().k1A  = model_params_linear.k1A;
                                    params[RName].back().tauA = model_params_linear.tauA;
                                    params[RName].back().k1V  = model_params_linear.k1V;
                                    params[RName].back().tauV = model_params_linear.tauV;
                                    params[RName].back().k2   = model_params_linear.k2;
                                    params[RName].back().RSS  = RSS;

                                }else if(HaveModel == Have_1Compartment2Input_5Param_Chebyshev_Model){
                                    params[RName].back().k1A  = model_params_cheby.k1A;
                                    params[RName].back().tauA = model_params_cheby.tauA;
                                    params[RName].back().k1V  = model_params_cheby.k1V;
                                    params[RName].back().tauV = model_params_cheby.tauV;
                                    params[RName].back().k2   = model_params_cheby.k2;
                                    params[RName].back().RSS  = RSS;
                                }
                                // ----------------------------------------------------------------------------
        
                            }//Loop over channels.
        
                        //If we're in the bounding box but not the ROI, do something.
                        }else{
                            //for(auto chan = 0; chan < first_img_it->channels; ++chan){
                            //    const auto curr_val = working.value(row, col, chan);
                            //    if(curr_val != 0) FUNCERR("There are overlapping ROI bboxes. This code currently cannot handle this. "
                            //                              "You will need to run the functor individually on the overlapping ROIs.");
                            //    working.reference(row, col, chan) = static_cast<float>(10);
                            //}
                        } // If is in ROI or ROI bbox.
                    } //Loop over cols
                } //Loop over rows
            } //Loop over ROIs.
        } //Loop over contour_collections.
    } //Loop over images in one of the collections.


    //Dump the data to file or std::cout.
    auto *OS = &(std::cout);
    std::ofstream FO(Filename);
    if(!Filename.empty()) OS = &(FO);

    for(const auto &apair : params){
        const auto ROIName = apair.first;

        *OS << "# Parameters for ROI '" << ROIName << "'" << std::endl;
        *OS << "# k1A, tauA, k1V, tauV, k2, RSS" << std::endl;

        for(const auto & shtl : apair.second){
            *OS << shtl.k1A  << Separator
                << shtl.tauA << Separator
                << shtl.k1V  << Separator
                << shtl.tauV << Separator
                << shtl.k2   << Separator
                << shtl.RSS;
            *OS << std::endl;
        }
    }

    return std::move(DICOM_data);
}
