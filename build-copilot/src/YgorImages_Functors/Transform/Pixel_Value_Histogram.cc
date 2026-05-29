
#include <cstdint>
#include <any>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../ConvenienceRoutines.h"
#include "YgorFilesDirs.h"   //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorPlot.h"
#include "YgorString.h"      //Needed for GetFirstRegex(...)

static std::map<uint32_t, std::vector<double>> pixel_vals;

static bool PixelHistogramAnalysisWasRun = false;


bool PixelHistogramAnalysis(planar_image_collection<float,double>::images_list_it_t  local_img_it,
                            std::list<std::reference_wrapper<planar_image_collection<float,double>>> ,
                            std::list<std::reference_wrapper<contour_collection<double>>> ccsl, 
                            std::any ){

    //This routine computes histograms of pixel values for the given ROIs.
    PixelHistogramAnalysisWasRun = true;


    //Figure out if there are any contours which are within the spatial extent of the image. 
    // There are many ways to do this! Since we are merely highlighting the contours, we scan 
    // all specified collections and treat them homogeneously.
    //
    // NOTE: We only bother to grab individual contours here. You could alter this if you wanted 
    //       each contour_collection's contours to have an identifying colour.
    if(ccsl.empty()) YLOGERR("Missing contour info needed for voxel colouring. Cannot continue");
    using contour_iter = std::list<contour_of_points<double> >::iterator;
    std::list<contour_iter> rois;
//std::list<contour_of_points<double>> rois;
    for(auto &ccs : ccsl){
        for(auto it =  ccs.get().contours.begin(); it != ccs.get().contours.end(); ++it){
            if(it->points.empty()) continue;
            //if(local_img_it->encompasses_contour_of_points(*it)) rois.emplace_back(*it);
            if(local_img_it->encompasses_contour_of_points(*it)) rois.push_back(it);
        }
    }

    //Loop over the rois, rows, columns, channels, and finally any selected images (if applicable).
    const auto row_unit   = local_img_it->row_unit;
    const auto col_unit   = local_img_it->col_unit;
    const auto ortho_unit = local_img_it->ortho_unit();

    uint32_t roi_numb = 0;
    for(const auto &roi : rois){
        ++roi_numb;
/*
        //Construct a bounding box to reduce computational demand of checking every voxel.
        auto BBox = roi->Bounding_Box_Along(row_unit, 1.0);
        auto BBoxBestFitPlane = BBox.Least_Squares_Best_Fit_Plane(vec3<double>(0.0,0.0,1.0));
        auto BBoxProjectedContour = BBox.Project_Onto_Plane_Orthogonally(BBoxBestFitPlane);
        const bool BBoxAlreadyProjected = true;
*/

        //Prepare a contour for fast is-point-within-the-polygon checking.
        auto BestFitPlane = roi->Least_Squares_Best_Fit_Plane(ortho_unit);
        auto ProjectedContour = roi->Project_Onto_Plane_Orthogonally(BestFitPlane);
//auto BestFitPlane = roi.Least_Squares_Best_Fit_Plane(orthog_unit);
//auto ProjectedContour = roi.Project_Onto_Plane_Orthogonally(BestFitPlane);
        const bool AlreadyProjected = true;


        for(auto row = 0; row < local_img_it->rows; ++row){
            for(auto col = 0; col < local_img_it->columns; ++col){
                //Figure out the spatial location of the present voxel.
                const auto point = local_img_it->position(row,col);

/*
                //Check if within the bounding box. It will generally be cheaper than the full contour (4 points vs. ? points).
                auto BBoxProjectedPoint = BBoxBestFitPlane.Project_Onto_Plane_Orthogonally(point);
                if(!BBoxProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BBoxBestFitPlane,
                                                                                    BBoxProjectedPoint,
                                                                                    BBoxAlreadyProjected)) continue;
*/

                //If we're in the bounding box, perform a more detailed check to see if we are in the ROI.
                auto ProjectedPoint = BestFitPlane.Project_Onto_Plane_Orthogonally(point);
                if(!ProjectedContour.Is_Point_In_Polygon_Projected_Orthogonally(BestFitPlane,
                                                                                ProjectedPoint,
                                                                                AlreadyProjected)) continue;
                for(auto chan = 0; chan < local_img_it->channels; ++chan){
                    const auto val = static_cast<double>(local_img_it->value(row, col, chan));
                    pixel_vals[roi_numb].push_back(val);
                }//Loop over channels.

            } //Loop over cols
        } //Loop over rows
    } //Loop over ROIs.

    //Alter the first image's metadata to reflect that averaging has occurred. You might want to consider
    // a selective whitelist approach so that unique IDs are not duplicated accidentally.
    UpdateImageDescription( std::ref(*local_img_it), "Pixel Histogram (images pass-through)" );

    return true;
}


//Dump lots of files and records.
void DumpPixelHistogramResults(){
    if(!PixelHistogramAnalysisWasRun){
        YLOGWARN("Forgoing dumping the pixel histogram analysis results; the analysis was not run");
        return;
    }

    std::string bd;
    for(auto i = 0; i < 1000; ++i){
        const std::string basedir = "/tmp/pet_ct_perfusion_"_s + Xtostring(i) + "/";
        if(  !Does_Dir_Exist_And_Can_Be_Read(basedir)
        &&   Create_Dir_and_Necessary_Parents(basedir) ){
            bd = basedir;
            break;
        }
    } 
    if(bd.empty()) YLOGERR("Unable to allocate a new directory for kitchen sink analysis output. Cannot continue");


    {
      const std::string title("Distribution of pixel intensities");

      Plotter2 toplot;
      toplot.Set_Global_Title(title);
      uint32_t roi_numb = 0;
      for(auto & buff : pixel_vals){
          samples_1D<double> binned = Bag_of_numbers_to_N_equal_bin_samples_1D_histogram(buff.second, buff.second.size()/10, true);
          toplot.Insert_samples_1D(binned, "Coefficients for ROI "_s + Xtostring(roi_numb), "lines");
          ++roi_numb;
      }
      toplot.Plot();
      const std::string basefname = bd + "binned_pixel_values_";
      toplot.Plot_as_PDF(Get_Unique_Sequential_Filename(basefname,6,".pdf"));
      WriteStringToFile(toplot.Dump_as_String(),
                         Get_Unique_Sequential_Filename(basefname,6,".dat"));
    }

    return;
}


