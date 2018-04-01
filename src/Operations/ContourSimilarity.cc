//ContourSimilarity.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <experimental/any>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"
#include "ContourSimilarity.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


std::list<OperationArgDoc> OpArgDocContourSimilarity(void){
    return std::list<OperationArgDoc>();
}

Drover ContourSimilarity(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //Get handles for each of the original image arrays so we can easily refer to them later.
    std::vector<std::shared_ptr<Image_Array>> orig_img_arrays;
    for(auto & img_arr : DICOM_data.image_data) orig_img_arrays.push_back(img_arr);

    //We require exactly one image volume. Use only the first image.
    if(orig_img_arrays.empty()) FUNCERR("This routine requires at least one imaging volume");
    orig_img_arrays.resize(1);

    //Package the ROIs of interest into two contour_collections to compare.
    contour_collection<double> cc_H; //Haley.
    contour_collection<double> cc_J; //Joel.
    contour_collection<double> cc_E; //Existing (i.e., therapist-contoured + oncologist inspected treatment contours).

    {
        for(auto & cc : DICOM_data.contour_data->ccs){
            for(auto & c : cc.contours){
                const auto name = c.metadata["ROIName"]; //c.metadata["NormalizedROIName"].
                const auto iccrH = GetFirstRegex(name, "(ICCR2016_Haley)");
                const auto iccrJ = GetFirstRegex(name, "(ICCR2016_Joel)");
                const auto eye  = GetFirstRegex(name, "([eE][yY][eE])");
                const auto orbit = GetFirstRegex(name, "([oO][rR][bB][iI][tT])");
                if(!iccrH.empty()){
                    cc_H.contours.push_back(c);
                }else if(!iccrJ.empty()){
                    cc_J.contours.push_back(c);
                }else if(!eye.empty() || !orbit.empty()){
                    FUNCWARN("Assuming contour '" << name << "' refers to eye(s)");
                    cc_E.contours.push_back(c);
                }
            }
        }

        if(cc_E.contours.empty()){
            FUNCWARN("Unable to find 'eyes' contour among:");
            for(auto & cc : DICOM_data.contour_data->ccs){
                std::cout << cc.contours.front().metadata["ROIName"] << std::endl;
            }
            throw std::domain_error("No 'eyes' contour available");
        }

    }

    //Compute similarity of the two contour_collections.
    ComputeContourSimilarityUserData ud;
    if(true){
        for(auto & img_arr : orig_img_arrays){
            ud.Clear();
            if(!img_arr->imagecoll.Compute_Images( ComputeContourSimilarity, { },
                                                   { std::ref(cc_H), std::ref(cc_E) }, &ud )){
                FUNCERR("Unable to compute Dice similarity");
            }
            std::cout << "Dice coefficient (H,E) = " << ud.Dice_Coefficient() << std::endl;
            std::cout << "Jaccard coefficient (H,E) = " << ud.Jaccard_Coefficient() << std::endl;

            ud.Clear();
            if(!img_arr->imagecoll.Compute_Images( ComputeContourSimilarity, { },
                                                   { std::ref(cc_J), std::ref(cc_E) }, &ud )){
                FUNCERR("Unable to compute Dice similarity");
            }
            std::cout << "Dice coefficient (J,E) = " << ud.Dice_Coefficient() << std::endl;
            std::cout << "Jaccard coefficient (J,E) = " << ud.Jaccard_Coefficient() << std::endl;

            ud.Clear();
            if(!img_arr->imagecoll.Compute_Images( ComputeContourSimilarity, { },
                                                   { std::ref(cc_H), std::ref(cc_J) }, &ud )){
                FUNCERR("Unable to compute Dice similarity");
            }
            std::cout << "Dice coefficient (H,J) = " << ud.Dice_Coefficient() << std::endl;
            std::cout << "Jaccard coefficient (H,J) = " << ud.Jaccard_Coefficient() << std::endl;
        }
    } 


    return DICOM_data;
}
