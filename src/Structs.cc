//Structs.cc.

#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cstdint>   //For int64_t.
#include <utility>   //For std::pair.
#include <algorithm> //std::min_element/max_element, std::stable_sort.
#include <tuple>
#include <array>
#include <functional>
#include <initializer_list>

#include <experimental/optional>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>

#include "YgorMisc.h"
#include "YgorStats.h"
#include "YgorString.h"
#include "YgorMath.h"
#include "YgorImages.h"
#include "YgorPlot.h"
#include "YgorDICOMTools.h" //For Is_File_A_DICOM_File(...);

#include "Structs.h"
#include "Imebra_Shim.h"

#include "Dose_Meld.h"

//This is a mapping from the segmentation history to a human-readable description.
// Try avoid using commas or tabs to make dumping as csv easier. This should in
// principle be bijective -- one should be able to take the description and go 
// backward to the segmentation history. But I haven't needed this functionality
// yet.
std::string Segmentations_to_Words(const std::vector<uint32_t> &in){
    std::string out;

    for(auto it = in.begin(); it != in.end(); ++it){
         if(*it == 0){
             out += "Original";
             continue;
         }

         //Splitting type.
         if(BITMASK_BITS_ARE_SET(*it,         Segmentations::volume)){
             out += " -> Split per-volume";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::height)){
             out += " -> Split per-height";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::core_peel)){
             out += " -> Core and Peeled";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::ray_cast)){
             out += " -> Ray Cast";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::contour)){    //Is this the appropriate spot for this?
             out += " -> Split per-contour";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::other_split)){
             out += " -> Custom split";
         }else{
             out += " -> (split type n/a)";
         }

         //Splitting planes/directions.
         if(BITMASK_BITS_ARE_SET(*it,         Segmentations::coronal)){
             out += " | cor";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::transverse)){
             out += " | tnv";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::sagittal)){
             out += " | sag";

//         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::medial)){
//             out += " | med";
//         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::lateral)){
//             out += " | lat";
         }else{
             out += " | (custom plane/dir)";
         }

         //Splitting orientations.
         if(BITMASK_BITS_ARE_SET(*it,         Segmentations::left)){
             out += " : left";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::right)){
             out += " : rght";

         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::front)){
             out += " : frnt";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::back)){
             out += " : back";

         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::top)){
             out += " : top";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::bottom)){
             out += " : btm";

         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::outer)){
             out += " : outer";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::inner)){
             out += " : inner";

         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::medial)){
             out += " : med";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::lateral)){
             out += " : lat";

         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::negative)){
             out += " : neg";
         }else if(BITMASK_BITS_ARE_SET(*it,   Segmentations::positive)){
             out += " : pos";
         }else{
             out += " : (part n/a)";
         }
    }
    return std::move(out);
}





//-----------------------------------------------------------------------------------------------------
//---------------------------------------- contours_with_meta -----------------------------------------
//-----------------------------------------------------------------------------------------------------
//Constructors.
contours_with_meta::contours_with_meta() : ROI_number(0), Minimum_Separation(-1.0), Segmentation_History( {0} ){ }

contours_with_meta::contours_with_meta(const contour_collection<double> &in){
    this->contours             = in.contours;
    this->ROI_number           = 0; //-1 instead?
    this->Minimum_Separation   = -1.0;
    this->Segmentation_History.push_back(0); //0 marks an original contour and may or may not be honored.
}

contours_with_meta::contours_with_meta(const contours_with_meta &rhs) : contour_collection<double>() {
    *this = rhs;
}

contours_with_meta & contours_with_meta::operator=(const contours_with_meta &rhs){
    if(this == &rhs) return *this;
    this->contours             = rhs.contours;
    this->ROI_number           = rhs.ROI_number;
    this->Raw_ROI_name         = rhs.Raw_ROI_name;
    this->Minimum_Separation   = rhs.Minimum_Separation;
    this->Segmentation_History = rhs.Segmentation_History;
    return *this;
}

//-----------------------------------------------------------------------------------------------------
//-------------------------------------------- Contour_Data -------------------------------------------
//-----------------------------------------------------------------------------------------------------
//Constructors.
Contour_Data::Contour_Data() { }
Contour_Data::Contour_Data(const Contour_Data &in) : ccs(in.ccs) { }

//Member functions.
void Contour_Data::operator=(const Contour_Data &rhs){
    if(this != &rhs) this->ccs = rhs.ccs;
    return;
}

//This routine produces a very simple, default plot of the entirety of the data. 
// If individual contour plots are required, use the contour_of_points::Plot() method instead.
void Contour_Data::Plot(void) const {
    Plotter a_plot;
    for(auto cc_it=this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        a_plot.ss << "# Default, simple plot for Contour with name '" << cc_it->Raw_ROI_name << "'" << std::endl;
        for(auto c_it=cc_it->contours.begin(); c_it != cc_it->contours.end(); ++c_it){
            for(auto p_it=c_it->points.begin(); p_it != c_it->points.end(); ++p_it){
                a_plot.ss << p_it->x << " ";
                a_plot.ss << p_it->y << " ";
                //a_plot.ss << (*p_it).z << " ";
                a_plot.ss << std::endl;
            }
            a_plot.Iterate_Linestyle();
        }
    }
    a_plot.Plot();
    return;
}


std::unique_ptr<Contour_Data> Contour_Data::Duplicate(void) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);
    *output = *this;
    return std::move(output);
}

//This function will split contour_collection units. It does not care about ROI number, ROI name, or height.
std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Volume_Along_Given_Plane_Unit_Normal(const vec3<double> &N) const {           //---- TODO: is this the correct plane, considering the patient DICOM orientation? (Check all files as test) 
    std::unique_ptr<Contour_Data> output (new Contour_Data);
    const uint32_t segmentation = ( Segmentations::other_orientation | Segmentations::volume );

    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        //In order to keep similar contours within the same contour_collection, we fill these buffers
        // with above/below data from a single contour_collection. We commit each buffer as a separate
        // contour_collection in the output. 
        contours_with_meta above, below;

        above.ROI_number            = cc_it->ROI_number;
        above.Minimum_Separation    = cc_it->Minimum_Separation;
        above.Raw_ROI_name          = cc_it->Raw_ROI_name;
        above.Segmentation_History  = cc_it->Segmentation_History;
        above.Segmentation_History.push_back(segmentation | Segmentations::positive); 

        below.ROI_number            = cc_it->ROI_number;
        below.Minimum_Separation    = cc_it->Minimum_Separation;
        below.Raw_ROI_name          = cc_it->Raw_ROI_name;
        below.Segmentation_History  = cc_it->Segmentation_History;
        below.Segmentation_History.push_back(segmentation | Segmentations::negative);

        //Generate the plane by cycling over all the contours in each contour_collection (ie. volume).
        const vec3<double> r = cc_it->Centroid();
        const plane<double> theplane(N,r);

        //Split each contour and push it into the proper buffer.
        for(auto c_it = cc_it->contours.begin(); c_it != cc_it->contours.end(); ++c_it){
            std::list<contour_of_points<double>> nclist = c_it->Split_Along_Plane(theplane);
            for(auto nc_it = nclist.begin(); nc_it != nclist.end(); ++nc_it){
                const auto rough_center = nc_it->First_N_Point_Avg(3); //Average_Point(); //Just need a point above or below.
                if(theplane.Is_Point_Above_Plane(rough_center)){
                    above.contours.push_back(*nc_it);
                }else{
                    below.contours.push_back(*nc_it);
                }
            }
        }
        output->ccs.push_back(std::move(above));
        output->ccs.push_back(std::move(below));
    }
    return std::move(output);
}

std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Volume_Along_Coronal_Plane(void) const {
    auto out(this->Split_Per_Volume_Along_Given_Plane_Unit_Normal(vec3<double>(0.0,1.0,0.0))); //Coronal plane.       ---- TODO: is this the correct plane, considering the patient DICOM orientation? (Check all files as test)

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto cc_it = out->ccs.begin(); cc_it != out->ccs.end(); ++cc_it){
        auto last_hist = --(cc_it->Segmentation_History.end());
        *last_hist |= Segmentations::coronal;
        if((*last_hist & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::front;   //Front (forward-facing direction) of the patient.
        }else{
            *last_hist |= Segmentations::back;    //Back of the patient.
        }
    }
    return std::move( out );
}

std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Volume_Along_Sagittal_Plane(void) const {
    auto out(this->Split_Per_Volume_Along_Given_Plane_Unit_Normal(vec3<double>(1.0,0.0,0.0))); //Sagittal plane.        ---- TODO: is this the correct plane, considering the patient DICOM orientation? (Check all files as test)

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto cc_it = out->ccs.begin(); cc_it != out->ccs.end(); ++cc_it){
        auto last_hist = --(cc_it->Segmentation_History.end());
        *last_hist |= Segmentations::sagittal;
        if((*last_hist & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::left;     //Leftward direction for the patient.
        }else{
            *last_hist |= Segmentations::right;    //Rightward direction for the patient.
        }
    }
    return std::move( out );
}

std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Volume_Along_Transverse_Plane(void) const {
    auto out(this->Split_Per_Volume_Along_Given_Plane_Unit_Normal(vec3<double>(0.0,0.0,1.0))); //Transverse plane.       ---- TODO: is this the correct plane, considering the patient DICOM orientation? (Check all files as test)

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto cc_it = out->ccs.begin(); cc_it != out->ccs.end(); ++cc_it){
        auto last_hist = --(cc_it->Segmentation_History.end());
        *last_hist |= Segmentations::transverse;
        if((*last_hist & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::bottom;  //Downward direction for the patient.
        }else{
            *last_hist |= Segmentations::top;     //Upward direction for the patient.
        }
    }
    return std::move( out );
}

/*
std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Height_Along_Given_Plane_Unit_Normal(const vec3<double> &N) const {
    //NOTE: This function is written in such a way that it assumes the data for any given ROI number is spread out over many
    // geographical locations (and in separate contour_collections). This is required, because, for example, if this routine
    // needs to sub-segment a sub-segment, it cannot treat the various pieces as a single volume.
    //
    //NOTE: A plane itself is not passed in because the point of plane intersection is taken as the average point.
    std::unique_ptr<Contour_Data> output (new Contour_Data);

    //Check if we have any contour numbers which are duplicated. This routine requires each contour_collection have
    // a distinct number.
    for(auto cc1_it = this->ccs.begin(); cc1_it != this->ccs.end(); ++cc1_it){
        for(auto cc2_it = cc1_it; cc2_it != this->ccs.end(); ++cc2_it){
            ++cc2_it;
            if(!(cc2_it != this->ccs.end())) continue; //Done!
            if(cc1_it->ROI_number != cc2_it->ROI_number){
                FUNCERR("This routine requires that distinct contour_collections have distinct ROI numbers");
            }
        }
    }
            
    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        //Store some data about the contour collection.
        const auto the_roi_numb(cc_it->ROI_number);
        const auto slice_thickness(cc_it->Minimum_Separation);


        //Cycle through the contours, computing the 'height'.
        std::set<decltype(this->ccs.begin())> used_contours, contours_to_use;
        for(auto c1_it = cc_it->contours.begin(); c1_it != cc_it->contours.end(); ++c1_it){
            used_contours.insert(contours_to_use.begin(), contours_to_use.end());
            contours_to_use.clear();
            if(!(used_contours.find(c1_it) != used_contours.end())) continue;

            vec3<double> Rave;
            long int numb_of_contours = 0;

            const double height1 = c1_it->Average_Point().Dot(vec3<double>(0.0,0.0,1.0));

            //Cycle through all contours looking for those at the same height. Include self.
            for(auto c2_it = cc_it->contours.begin(); c2_it != cc_it->contours.end(); ++c2_it){
                if(!(used_contours.find(c2_it) != used_contours.end())) continue;

                const double height2 = c2_it->Average_Point().Dot(vec3<double>(0.0,0.0,1.0));
                if(YGORABS(height2-height1) >= slice_thickness) continue;
               
                ++numb_of_contours;
                Rave += c2_it->Centroid(); //Average_Point();
                contours_to_use.insert(c2_it);
            }
            Rave /= static_cast<double>(numb_of_contours);

            //Construct the plane we are going to split on.
            const plane<double> theplane(N,Rave);

            //Create the contour_collection we will 


            //Send each contour off to the exploder routine individually.
            for(auto ctu_it = contours_to_use.begin(); ctu_it != contours_to_use.end(); ++ctu_it){

                //These are the newly-split, but meta-data-less contour_of_points.
                const auto nclist = ctu_it->Split_Along_Plane(theplane);
                std::list<contours_with_meta> cwmlist;

                for(auto ncc_it = ncclist.begin(); ncc_it != ncclist.end(); ++ncc_it){
                    contours_with_meta shtl(*ncc_it);
    
                    //Metadata inherited directly from the mother.
                    shtl.ROI_number           = cc_it->ROI_number;
                    shtl.Minimum_Separation   = cc_it->Minimum_Separation;
                    shtl.Raw_ROI_name         = cc_it->Raw_ROI_name;
                    shtl.Segmentation_History = cc_it->Segmentation_History;
    
                    //Now we add a generic history element.
                    uint32_t segmentation = (Segmentations::other_orientation | Segmentations::volume);
                    const auto rc = ncc_it->Average_Point(); //Just need a point above or below.
                    if(theplane.Is_Point_Above_Plane(rc)){
                        segmentation |= Segmentations::positive;
                    }else{
                        segmentation |= Segmentations::negative;
                    }
                    shtl.Segmentation_History.push_back(segmentation);
    
                    cwmlist.push_back(std::move(shtl));
                }
                output->ccs.insert(output->ccs.end(), cwmlist.begin(), cwmlist.end());


            }

        }



        //Send each contour individually off to the exploder routine.
        for(auto c_it=this->contours.begin(); c_it!=this->contours.end(); ++c_it){
            if( ((*c_it).ROI_number == the_roi_number) && ((*c_it).points.front().z == height) ){

                //This code turns a list of contour_of_points into a list of contour_with_meta. 
                std::list< contour_of_points<double> > temp = (*c_it).Split_Along_Plane( theplane );
                std::list< contour_with_meta > temp2;
    
                //Label each contour accordingly.
                for(auto nc_it = temp.begin(); nc_it != temp.end(); ++nc_it){
                    contour_with_meta temp3( (*nc_it) );

                    //This is inherited directly from the mother contour.
                    temp3.ROI_number           = (*c_it).ROI_number;
                shtl.Minimum_Separation   = cc_it->Minimum_Separation;
                    temp3.Raw_ROI_name         = (*c_it).Raw_ROI_name;
                    temp3.Segmentation_History = (*c_it).Segmentation_History;  //This is the mother's segmentation history. We append to it shortly.

                    //Now we add a generic segmentation history step. We add simple orientation info which may be supplemented by more 
                    // specific calling functions.
                    uint32_t segmentation = ( Segmentations::other_orientation | Segmentations::volume );
                    const auto rough_center = nc_it->Average_Point(); //Just need a point above or below.
                    if( theplane.Is_Point_Above_Plane( rough_center ) ){
                        segmentation |= Segmentations::positive;
                    }else{
                        segmentation |= Segmentations::negative;
                    }
                    temp3.Segmentation_History.push_back(segmentation);
     
                    temp2.push_back( temp3 );
                }

                output->contours.insert( output->contours.end(), temp2.begin(), temp2.end() );
            }
        }
    }

    return std::move(output);
}

std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Height_Along_Coronal_Plane(void) const {
    auto out( this->Split_Per_Height_Along_Given_Plane_Unit_Normal(vec3<double>( 0.0, 1.0, 0.0 )) ); //Coronal plane.

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto c_it=out->contours.begin(); c_it != out->contours.end(); ++c_it){
        auto last_hist = --(c_it->Segmentation_History.end());
        *last_hist |= Segmentations::coronal;
        if(((*last_hist) & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::front;   //Front (forward-facing direction) of the patient.
        }else{
            *last_hist |= Segmentations::back;    //Back of the patient.
        }
    }
    return std::move( out );
}

std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Height_Along_Sagittal_Plane(void) const {
    auto out( this->Split_Per_Height_Along_Given_Plane_Unit_Normal(vec3<double>( 1.0, 0.0, 0.0 )) ); //Sagittal plane.

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto c_it=out->contours.begin(); c_it != out->contours.end(); ++c_it){
        auto last_hist = --(c_it->Segmentation_History.end());
        *last_hist |= Segmentations::sagittal;
        if(((*last_hist) & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::left;     //Leftward direction for the patient.
        }else{
            *last_hist |= Segmentations::right;    //Rightward direction for the patient.
        }
    }
    return std::move( out );
}
*/


std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Contour_Along_Given_Plane_Unit_Normal(const vec3<double> &N) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);
    const uint32_t segmentation = ( Segmentations::other_orientation | Segmentations::contour );

    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        //In order to keep similar contours within the same contour_collection, we fill these buffers
        // with above/below data from a single contour_collection. We commit each buffer as a separate
        // contour_collection in the output. 
        contours_with_meta above, below;

        above.ROI_number            = cc_it->ROI_number;
        above.Minimum_Separation    = cc_it->Minimum_Separation;
        above.Raw_ROI_name          = cc_it->Raw_ROI_name;
        above.Segmentation_History  = cc_it->Segmentation_History;
        above.Segmentation_History.push_back(segmentation | Segmentations::positive); 

        below.ROI_number            = cc_it->ROI_number;
        below.Minimum_Separation    = cc_it->Minimum_Separation;
        below.Raw_ROI_name          = cc_it->Raw_ROI_name;
        below.Segmentation_History  = cc_it->Segmentation_History;
        below.Segmentation_History.push_back(segmentation | Segmentations::negative);

        for(auto c_it = cc_it->contours.begin(); c_it != cc_it->contours.end(); ++c_it){
            const vec3<double> r = c_it->Centroid(); //Average_Point();
            const plane<double> theplane(N,r);

            //Split each contour and push it into the proper buffer.
            std::list<contour_of_points<double>> nclist = c_it->Split_Along_Plane(theplane);
            for(auto nc_it = nclist.begin(); nc_it != nclist.end(); ++nc_it){
                const auto rough_center = nc_it->First_N_Point_Avg(3); //Average_Point(); //Just need a point above or below.
                if(theplane.Is_Point_Above_Plane(rough_center)){
                    above.contours.push_back(*nc_it);
                }else{
                    below.contours.push_back(*nc_it);
                }
            }
        }
        output->ccs.push_back(std::move(above));
        output->ccs.push_back(std::move(below));
    }
    return std::move(output);
}

std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Contour_Along_Coronal_Plane(void) const {
    auto out(this->Split_Per_Contour_Along_Given_Plane_Unit_Normal(vec3<double>(0.0,1.0,0.0))); //Coronal plane.

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto cc_it = out->ccs.begin(); cc_it != out->ccs.end(); ++cc_it){
        auto last_hist = --(cc_it->Segmentation_History.end());
        *last_hist |= Segmentations::coronal;
        if(((*last_hist) & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::front;   //Front (forward-facing direction) of the patient.
        }else{
            *last_hist |= Segmentations::back;    //Back of the patient.
        }
    }
    return std::move( out );
}

std::unique_ptr<Contour_Data>  Contour_Data::Split_Per_Contour_Along_Sagittal_Plane(void) const {
    auto out(this->Split_Per_Contour_Along_Given_Plane_Unit_Normal(vec3<double>(1.0,0.0,0.0))); //Sagittal plane.

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto cc_it = out->ccs.begin(); cc_it != out->ccs.end(); ++cc_it){
        auto last_hist = --(cc_it->Segmentation_History.end());
        *last_hist |= Segmentations::sagittal;
        if(((*last_hist) & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::left;     //Leftward direction for the patient.
        }else{
            *last_hist |= Segmentations::right;    //Rightward direction for the patient.
        }
    }
    return std::move( out );
}


//-------------------------
//------ Ray-Casting ------
//-------------------------
//This will split contours AGAINST the given U. In other words, the ray cast happens along U. The "top" is the portion
// which is between the beginning and middle of the ray (from contour's edge to edge).
std::unique_ptr<Contour_Data>  Contour_Data::Raycast_Split_Per_Contour_Against_Given_Direction(const vec3<double> &U) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);
    const uint32_t segmentation = ( Segmentations::other_orientation | Segmentations::contour | Segmentations::ray_cast );

    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        //In order to keep similar contours within the same contour_collection, we fill these buffers
        // with above/below data from a single contour_collection. We commit each buffer as a separate
        // contour_collection in the output. 
        contours_with_meta above, below;

        above.ROI_number            = cc_it->ROI_number;
        above.Minimum_Separation    = cc_it->Minimum_Separation;
        above.Raw_ROI_name          = cc_it->Raw_ROI_name;
        above.Segmentation_History  = cc_it->Segmentation_History;
        above.Segmentation_History.push_back(segmentation | Segmentations::positive);

        below.ROI_number            = cc_it->ROI_number;
        below.Minimum_Separation    = cc_it->Minimum_Separation;
        below.Raw_ROI_name          = cc_it->Raw_ROI_name;
        below.Segmentation_History  = cc_it->Segmentation_History;
        below.Segmentation_History.push_back(segmentation | Segmentations::negative);

        for(auto c_it = cc_it->contours.begin(); c_it != cc_it->contours.end(); ++c_it){
            const auto r = c_it->Centroid(); //Average_Point();
            const plane<double> theplane(U.unit(),r);
            //Split each contour and push it into the proper buffer.
            std::list<contour_of_points<double>> nclist = c_it->Split_Against_Ray(U.unit());
            for(auto nc_it = nclist.begin(); nc_it != nclist.end(); ++nc_it){
                const auto rough_center = nc_it->First_N_Point_Avg(3); //Average_Point(); //Just need a point above or below.
                if(theplane.Is_Point_Above_Plane(rough_center)){
                    above.contours.push_back(*nc_it);
                }else{
                    below.contours.push_back(*nc_it);
                }
            }
        }
        output->ccs.push_back(std::move(above));
        output->ccs.push_back(std::move(below));
    }
    return std::move(output);
}

std::unique_ptr<Contour_Data>  Contour_Data::Raycast_Split_Per_Contour_Into_ANT_POST(void) const {
    auto out(this->Raycast_Split_Per_Contour_Against_Given_Direction(vec3<double>(1.0,0.0,0.0)));

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto cc_it = out->ccs.begin(); cc_it != out->ccs.end(); ++cc_it){
        auto last_hist = --(cc_it->Segmentation_History.end());
        *last_hist |= Segmentations::ant_post;
        if(((*last_hist) & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::back;   //Back (from patient perspective) of the patient.
        }else{
            *last_hist |= Segmentations::front;  //Front of the patient.
        }
    }
    return std::move( out );
}


std::unique_ptr<Contour_Data>  Contour_Data::Raycast_Split_Per_Contour_Into_Lateral(void) const {
    auto out(this->Raycast_Split_Per_Contour_Against_Given_Direction(vec3<double>(0.0,1.0,0.0)));

    //Update the segmentation history of the new contour. We augment the generic one with additional information.
    for(auto cc_it = out->ccs.begin(); cc_it != out->ccs.end(); ++cc_it){
        auto last_hist = --(cc_it->Segmentation_History.end());
        *last_hist |= Segmentations::lateral;
        if(((*last_hist) & Segmentations::negative) == Segmentations::negative){
            *last_hist |= Segmentations::left;   //Left side (from patient perspective) of the patient.
        }else{
            *last_hist |= Segmentations::right;  //Right side of the patient.
        }
    }
    return std::move( out );
}


//Core and Peel splitting. Uses the cc centroid.
std::unique_ptr<Contour_Data> Contour_Data::Split_Core_and_Peel(double frac_dist) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);
    const uint32_t segmentation = ( Segmentations::other_orientation | Segmentations::contour | Segmentations::core_peel );

    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        //In order to keep similar contours within the same contour_collection, we fill these buffers
        // with above/below data from a single contour_collection. We commit each buffer as a separate
        // contour_collection in the output. 
        contours_with_meta core, peel;

        core.ROI_number            = cc_it->ROI_number;
        core.Minimum_Separation    = cc_it->Minimum_Separation;
        core.Raw_ROI_name          = cc_it->Raw_ROI_name;
        core.Segmentation_History  = cc_it->Segmentation_History;
        core.Segmentation_History.push_back(segmentation | Segmentations::inner);

        peel.ROI_number            = cc_it->ROI_number;
        peel.Minimum_Separation    = cc_it->Minimum_Separation;
        peel.Raw_ROI_name          = cc_it->Raw_ROI_name;
        peel.Segmentation_History  = cc_it->Segmentation_History;
        peel.Segmentation_History.push_back(segmentation | Segmentations::outer);

//        const auto r = cc_it->Centroid();
        auto split = cc_it->Split_Into_Core_Peel_Spherical(frac_dist);  

        for(auto s_c_it = split.back().contours.begin(); s_c_it != split.back().contours.end(); ++s_c_it){
            if(s_c_it->points.size() >= 3) peel.contours.push_back(*s_c_it);
        }
        for(auto s_c_it = split.front().contours.begin(); s_c_it != split.front().contours.end(); ++s_c_it){
            if(s_c_it->points.size() >= 3) core.contours.push_back(*s_c_it);
        }

/*
        for(auto c_it = cc_it->contours.begin(); c_it != cc_it->contours.end(); ++c_it){
            const auto r = c_it->Centroid(); //Average_Point();
            const plane<double> theplane(U.unit(),r);
            //Split each contour and push it into the proper buffer.
            std::list<contour_of_points<double>> nclist = c_it->Split_Against_Ray(U.unit());
            for(auto nc_it = nclist.begin(); nc_it != nclist.end(); ++nc_it){
                const auto rough_center = nc_it->First_N_Point_Avg(3); //Average_Point(); //Just need a point above or below.
                if(theplane.Is_Point_Above_Plane(rough_center)){
                    above.contours.push_back(*nc_it);
                }else{
                    below.contours.push_back(*nc_it);
                }
            }
        }
*/
        if(!(core.contours.empty())) output->ccs.push_back(std::move(core));
        if(!(peel.contours.empty())) output->ccs.push_back(std::move(peel));
    }
    return std::move(output);
}

//--------------------------
//--- Geometric ordering ---
//--------------------------
//Take sub-segments which have been split into left / right and reorder and relabel them to 
// denote medial and lateral order. This is useful for comparing subsegments of laterally-symmetric
// organs. The subsegment numbers will be reordered to reflect the symmetry:
//
//   Left Par      Right Par            Left Par      Right Par
//    _______       _______              _______       _______ 
//   | 1 | 2 |  +  | 1 | 2 |            | 1 | 2 |  +  | 2 | 1 |
//   |___|___|  +  |___|___|     ===>   |___|___|  +  |___|___|
//   | 3 | 4 |  +  | 3 | 4 |            | 3 | 4 |  +  | 4 | 3 |
//   |___|___|  +  |___|___|            |___|___|  +  |___|___| 
//
//            where the +'s denote the line of symmetry specified . 
//
//Given the plane, all subsegments with (left) are compared with the (right)'s distance from the plane.
// Whichever are further is classified as the (lateral) and the others become (medial). 
// Then the ordering of the medial and lateral parts are rearranged to ensure the lateral occurs first.
//
//NOTE: This routine ONLY works on the MOST RECENT splitting!
//
//NOTE: This routine assumes the left and right are grouped together in memory, like:
//             [ ... ][ ... ][ left ][ left ][ left ][ right ][ right ][ ... ]
//      (where either left or right may occur first).
//
//NOTE: This routine will put the LATERAL subsegment first in memory.
std::unique_ptr<Contour_Data> Contour_Data::Reorder_LR_to_ML(void) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);
    if(this->ccs.empty()) return std::move(output);

    //To simplify the tediousness a bit, we will assume a sagittal plane.
    // We will also choose the plane center to be the average of a contour centroids.
    // We could optionally pass these in, but it sucks to have to breakout and compute
    // this stuff everytime it is needed. I think this will work for L and R parotids 
    // at least.
    vec3<double> sag_N(1.0,0.0,0.0); //Sagittal plane.
    vec3<double> avg_P(0.0,0.0,0.0);
    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        avg_P += cc_it->Centroid();    
    }
    avg_P /= static_cast<double>(this->ccs.size());
    const plane<double> Plane(sag_N,avg_P);

    //Cycle over the contours.
    auto cc_it = this->ccs.begin();
//    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
    while(cc_it != this->ccs.end()){
        auto prev_hist = cc_it->Segmentation_History;

        //Check if there is NO subsegmentation. (Is the original counted as history?)
        if(prev_hist.empty()){
            output->ccs.push_back( *cc_it );
            ++cc_it;
            continue;
        }

        auto last_hist = --(cc_it->Segmentation_History.end());

        //Push the unaffected into new storage in the same order.
        if( !(((*last_hist) & Segmentations::left ) == Segmentations::left )
        &&  !(((*last_hist) & Segmentations::right) == Segmentations::right) ){
            output->ccs.push_back( *cc_it );
            ++cc_it;
            continue;
        }

        prev_hist.pop_back(); //Now contains all but the last history.

        //Now read ahead in the collection, looking for all others with identical
        // prev_history and the same last_hist.
        auto cc2_it = std::next(cc_it,1);
        while(cc2_it != this->ccs.end()){
            if(cc_it->Segmentation_History != cc2_it->Segmentation_History){
                break;
            }
            ++cc2_it;
        }
        //cc2_it now points to the first non-similar subsegment. Do the same with it.
        auto cc3_it = std::next(cc2_it,1);
        while(cc3_it != this->ccs.end()){
            if(cc2_it->Segmentation_History != cc3_it->Segmentation_History){
                break;
            }
            ++cc3_it;
        }
        //cc3_it now points to an unrelated subsegment.

        //Determine the distance from the plane for each group. 
        vec3<double> RA(0.0,0.0,0.0), RB(0.0,0.0,0.0);
        {
          auto ccD_it = std::next(cc_it,0);
          double N = 0.0;
          while(ccD_it != cc2_it){
              N += 1.0;
              RA += ccD_it->Centroid();
              ++ccD_it;
          }
          RA /= N;
//          ccD_it = std::next(cc2_it,0);
          N = 0.0;
          while(ccD_it != cc3_it){
              N += 1.0;
              RB += ccD_it->Centroid();
              ++ccD_it;
          }
          RB /= N;
        }
        const double sDA = Plane.Get_Signed_Distance_To_Point(RA);
        const double sDB = Plane.Get_Signed_Distance_To_Point(RB);

//        size_t Curr_Size = output->ccs.size();
        decltype(output->ccs) newlist;

        //Copy the elements into a sorting buffer and alter their segmentation history.
        {
//          auto ccD_it = std::next(cc_it,0);
//          while(ccD_it != cc2_it){
          while(cc_it != cc2_it){
              auto last = cc_it->Segmentation_History.back();
              if(YGORABS(sDA) > YGORABS(sDB)){ last |= Segmentations::lateral; 
              }else{                           last |= Segmentations::medial;
              }
              last = (last & ~Segmentations::left);  //Get rid of left.
              last = (last & ~Segmentations::right); //Get rid of right.
              newlist.push_back( *cc_it );
              newlist.back().Segmentation_History.back() = last;
              ++cc_it;
          }
          while(cc_it != cc3_it){
              auto last = cc_it->Segmentation_History.back();
              if(YGORABS(sDA) > YGORABS(sDB)){ last |= Segmentations::medial;
              }else{                           last |= Segmentations::lateral;
              }
//              last |= Segmentations::medial;
              last = (last & ~Segmentations::left);  //Get rid of left.
              last = (last & ~Segmentations::right); //Get rid of right.
              newlist.push_back( *cc_it );
              newlist.back().Segmentation_History.back() = last;
              //ccD_it->Segmentation_History.back() = last;
              ++cc_it;
          }
        }

        //Now sort them in-place, preserving the order of matching elements.
        auto ascender = [=](const contours_with_meta &A, const contours_with_meta &B) -> bool {
            const auto Alast = A.Segmentation_History.back();
            const auto Blast = B.Segmentation_History.back();
            const bool Aismed = (Alast & Segmentations::medial) == Segmentations::medial;
            const bool Bismed = (Blast & Segmentations::medial) == Segmentations::medial;

            if(Aismed == Bismed) return false;
            return Bismed;
            //if(Aismed) return false;
            //return true; //Bismed == true; 
        }; 
        newlist.sort(ascender); //Note std::list.sort() is a stable_sort!
   
        //Now insert them into the output.
        for(auto it = newlist.begin(); it != newlist.end(); ++it){
            output->ccs.push_back( *it );
        }

        //Now insert them, advancing the iter.
//        while(cc_it != cc3_it) output->ccs.push_back( *cc_it );
    }

    return std::move(output);
}


//-------------------------
//---- Selector Members ---
//-------------------------

//Extracts a single contours_with_meta at list position N.
//
//NOTE: Returns a nullptr if the designated contours_with_meta doesn't exist.
std::unique_ptr<Contour_Data> Contour_Data::Get_Contours_Number(long int N) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);
    if((N < 0) || (N >= static_cast<long int>(this->ccs.size()))) return nullptr;

    auto cc_it = std::next(this->ccs.begin(), N);
    output->ccs.push_back( *cc_it );
    return std::move(output);
}


//Extracts single contour M from contours_with_meta at list position N.
// Puts contour inside its own std::unique_ptr<Contour_Data> so it can easily be used for DVHs, etc.
//
//NOTE: Returns a nullptr if the designated contour doesn't exist.
std::unique_ptr<Contour_Data> Contour_Data::Get_Single_Contour_Number(long int N, long int M) const {
    std::unique_ptr<Contour_Data> output( this->Get_Contours_Number(N) );
    if((output == nullptr) || (output->ccs.size() != 1)) return nullptr;

    if((M < 0) || (M >= static_cast<long int>(output->ccs.front().contours.size()))) return nullptr;

    auto c_it_to_keep = std::next(output->ccs.front().contours.begin(), M);
    auto c_it = output->ccs.front().contours.begin(); 
    while(c_it != output->ccs.front().contours.end()){
        if(c_it != c_it_to_keep){
            c_it = output->ccs.front().contours.erase(c_it);
        }else{
            ++c_it;
        }
    }
    return std::move(output);
}



std::unique_ptr<Contour_Data> Contour_Data::Get_Contours_With_Numbers(const std::set<long int> &in) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);

    for(auto n_it = in.begin(); n_it != in.end(); ++n_it){
        //Cycle over the contours, checking each against the input.
        for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
            if(cc_it->ROI_number == *n_it) output->ccs.push_back( *cc_it );
        }
    }

    if(output->ccs.empty()) FUNCWARN("No data was pushed into the contour - maybe there is no structure with the desired contour number(s)?");
    return std::move(output);
}

std::unique_ptr<Contour_Data> Contour_Data::Get_Contours_With_Number(long int in) const {
    //Simply construct a set with one element and pass it to the multiple-number routine.
    return std::move(this->Get_Contours_With_Numbers({ in }));
}

std::unique_ptr<Contour_Data> Contour_Data::Get_Contours_With_Last_Segmentation(const uint32_t &in) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);

    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        if(cc_it->Segmentation_History.empty()) continue;

        auto last_hist = --(cc_it->Segmentation_History.end());
        if((*last_hist & in) == in) output->ccs.push_back( *cc_it );
    }

    if(output->ccs.empty()) FUNCWARN("No data was pushed into the contour - maybe there is no structure with the desired contour number(s)?");
    return std::move(output);
}


std::unique_ptr<Contour_Data> Contour_Data::Get_Contours_With_Segmentation(const std::set<uint32_t> &in) const {
    std::unique_ptr<Contour_Data> output (new Contour_Data);

    for(auto cc_it = this->ccs.begin(); cc_it != this->ccs.end(); ++cc_it){
        size_t counter = 0;

        //Cycle over the contours' segmentation history.
        for(auto sh_it = cc_it->Segmentation_History.begin(); sh_it != cc_it->Segmentation_History.end(); ++sh_it){
            //Cycle over the criteria.
            for(auto cr_it = in.begin(); cr_it != in.end(); ++cr_it){
                //Check if the criteria and the segmentation history match. If so, increment the counter.
                if( ((*cr_it) & (*sh_it)) == (*cr_it) ){
                    ++counter;
                }
            }
        }

        //Check if all criteria have been satisfied. If so, push back.
        if(counter == in.size()) output->ccs.push_back( *cc_it );
    }

    if(output->ccs.empty()) FUNCWARN("No data was pushed into the contour - maybe there is no structure with the desired contour number(s)?");
    return std::move(output);
}




/*
//-----------------------------------------------------------------------------------------------------
//--------------------------------------- planar_image_with_meta --------------------------------------
//-----------------------------------------------------------------------------------------------------
//Constructors.
planar_image_with_meta::planar_image_with_meta(){
    this->bits = 0;
    this->grid_scale = -1.0;
    this->frame_num  = -1;
    //Should call the planar_image() constructor automatically!
};

planar_image_with_meta::planar_image_with_meta( const planar_image<unsigned int,double> &in ){
    //Copy the image using the copy operator.
    (*this) = in;

    //Copy the additional meta information this class provides.
    this->
}

planar_image_with_meta( const planar_image_with_meta &in ){



}
*/
//planar_image_with_meta & planar_image_with_meta::operator=(.....)

/*
//-----------------------------------------------------------------------------------------------------
//----------------------------------------- Pixel Data stuff ------------------------------------------
//-----------------------------------------------------------------------------------------------------

unsigned int Pixel_Data::index(unsigned int i, unsigned int j, unsigned int k){
    //return k*columns*rows*channels + i*columns*channels + j*channels;
    return channels*( columns*( rows*k + i ) + j );
}

float Pixel_Data::clamped_channel(unsigned int channel, unsigned int i, unsigned int j, unsigned int k){
    return static_cast<float>( data[ index(i,j,k) + channel ]) / static_cast<float>( max[ channel ] );
}
unsigned int Pixel_Data::channel(unsigned int channel, unsigned int i, unsigned int j, unsigned int k){
    return data[ index(i,j,k) + channel ];
}

float Pixel_Data::x(unsigned int i, unsigned int j, unsigned int k){
    return  image_pos_x + static_cast<float>(i)*pixel_dx;
}
float Pixel_Data::y(unsigned int i, unsigned int j, unsigned int k){
    return  image_pos_y + static_cast<float>(j)*pixel_dy;
}
float Pixel_Data::z(unsigned int i, unsigned int j, unsigned int k){
    return  -slice_height + static_cast<float>(k)*pixel_dz;   //Using image_pos_z seems to be troublesome..
}

float Pixel_Data::ortho_inner(unsigned int k){   //For OpenGL, negate this function. For filtering layers, do not negate. See Overlay_Pixel_Data.cc.
    return -( slice_height - 0.5*pixel_dz - static_cast<float>(k)*pixel_dz );
}
float Pixel_Data::ortho_outer(unsigned int k){
    return -( slice_height + 0.5*pixel_dz - static_cast<float>(k)*pixel_dz );
}

*/

//---------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------- Dose_Array ------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------
Dose_Array::Dose_Array(){
    this->bits = 0;
    this->grid_scale = -1.0;
}

Dose_Array::Dose_Array(const Image_Array &rhs){
    //Performs a deep copy.
    this->imagecoll  = rhs.imagecoll;
    this->bits       = rhs.bits;
    this->grid_scale = 1.0;
    this->filename   = rhs.filename;
}

Dose_Array & Dose_Array::operator=(const Dose_Array &rhs){
    if(this == &rhs) return *this;
    this->imagecoll  = rhs.imagecoll; //Performs a deep copy (unless copying self).
    this->bits       = rhs.bits;
    this->grid_scale = rhs.grid_scale;
    this->filename   = rhs.filename;
    return *this;
}



//---------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------- Image_Array ------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------
Image_Array::Image_Array(){
    this->bits = 0;
}

Image_Array::Image_Array(const Image_Array &rhs){
    *this = rhs; //Performs a deep copy (unless copying self).
}

Image_Array::Image_Array(const Dose_Array &rhs){
    //Performs a deep copy (unless copying self).
    this->imagecoll  = rhs.imagecoll;
    this->bits       = rhs.bits;
    this->filename   = rhs.filename;
    //We drop and ignore the grid_scale member.
}

Image_Array & Image_Array::operator=(const Image_Array &rhs){
    //Performs a deep copy (unless copying self).
    if(this == &rhs) return *this;
    this->imagecoll  = rhs.imagecoll;
    this->bits       = rhs.bits;
    this->filename   = rhs.filename;
    return *this;
}


//---------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------ Drover -------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------

//Helper functions.
drover_bnded_dose_mean_dose_map_t drover_bnded_dose_mean_dose_map_factory(void){
    drover_bnded_dose_mean_dose_map_t out(/*25,*/ bnded_dose_map_cmp_lambda);
    return std::move(out);
}
drover_bnded_dose_centroid_map_t drover_bnded_dose_centroid_map_factory(void){
    drover_bnded_dose_centroid_map_t out(/*25,*/ bnded_dose_map_cmp_lambda);
    return std::move(out);
}
drover_bnded_dose_bulk_doses_map_t drover_bnded_dose_bulk_doses_map_factory(void){
    drover_bnded_dose_bulk_doses_map_t out(/*25,*/ bnded_dose_map_cmp_lambda);
    return std::move(out);
}
drover_bnded_dose_accm_dose_map_t drover_bnded_dose_accm_dose_map_factory(void){
    drover_bnded_dose_accm_dose_map_t out(/*25, */bnded_dose_map_cmp_lambda);
    return std::move(out);
}
drover_bnded_dose_min_max_dose_map_t drover_bnded_dose_min_max_dose_map_factory(void){
    drover_bnded_dose_min_max_dose_map_t out(/*25, */bnded_dose_map_cmp_lambda);
    return std::move(out);
}
drover_bnded_dose_min_mean_max_dose_map_t drover_bnded_dose_min_mean_max_dose_map_factory(void){
    drover_bnded_dose_min_mean_max_dose_map_t out(/*25, */bnded_dose_map_cmp_lambda);
    return std::move(out);
}
drover_bnded_dose_min_mean_median_max_dose_map_t drover_bnded_dose_min_mean_median_max_dose_map_factory(void){
    drover_bnded_dose_min_mean_median_max_dose_map_t out(/*25, */bnded_dose_map_cmp_lambda);
    return std::move(out);
}
drover_bnded_dose_pos_dose_map_t drover_bnded_dose_pos_dose_map_factory(void){
    drover_bnded_dose_pos_dose_map_t out(/*25, */bnded_dose_map_cmp_lambda);
    return std::move(out);
}
drover_bnded_dose_stat_moments_map_t drover_bnded_dose_stat_moments_map_factory(void){
    drover_bnded_dose_stat_moments_map_t out(/*25, */bnded_dose_map_cmp_lambda);
    return std::move(out);
}



//Constructors.
Drover::Drover(){
    Has_Been_Melded = false;
};
Drover::Drover( const Drover &in ) : contour_data(in.contour_data), dose_data(in.dose_data), image_data(in.image_data) {
    Has_Been_Melded = false;
};

//Member functions.
void Drover::operator=(const Drover &rhs){
    if(this != &rhs){
        //this->Has_Been_Melded = false;
        this->Has_Been_Melded = rhs.Has_Been_Melded;
        this->contour_data    = rhs.contour_data;
        this->dose_data       = rhs.dose_data;
        this->image_data      = rhs.image_data;
    }
    return;
}

/*
//This routine works fine. It groups everything in terms of ROI numbers instead of contour collections. This makes it unsuitable
// for operating on sub-segmented structures (which have identical roi numbers) unless some fancy footwork is done.
//
//In my opinion, currently it is more flexible to group things in terms of contour_collections. One limitation of this is that
// it will be impossible to compute bounded doses for compound structures (eg. left and right parotid together) unless they are
// both placed in the same contour collection (and the roi number is thus disposed of). In some sense, this is a fairly logical
// thing to do: if two independent contours are merged, then the resulting structure should at least have a unique roi number.
//
//Anyways, the functionality of this routine have been moved into the replacement routine below. This version may be deleted
// if no need exists for this version after some reasonable period of time.
//

void Drover::Bounded_Dose_General(std::list<double> *pixel_doses, std::map<long int,double> *mean_doses) const {
    //This function is a general routine for working with pixels bounded by contour data. It *might* be better to stick it in 
    // the contour or pixel classes, but it seems better (at the moment) to place it in the Drover class, where we have clearly
    // indicated which contour, which dose pixels, and which CT data we want to work with.
    //
    //This function should (probably) only be called internally. Use the wrapper member functions. There is no reason why one
    // couldn't use this function, however, so do whatever you want to!
    //
    //Output options:
    //  std::list<double> *pixel_doses;            <-- Holds the each voxel's dose. Discards spatial info about voxels.
    //  std::map<long int,double> *mean_doses;     <-- Holds the mean dose for each ROI contour number within the contour data.
    //
    // Pass a pointer to the desired container to compute the desired quantities.

    //----------------------------------------- Sanity/Safety Checks ----------------------------------------
    if(!this->Has_Been_Melded){
        FUNCERR("Attempted to use bounded dose routine without melding data. It is required to do so");
    }
    if((pixel_doses == nullptr) && (mean_doses == nullptr)){
        FUNCWARN("No valid output pointers provided. Nothing will be computed");
        return;
    }
    if(!this->Has_Contour_Data() || !this->Has_Dose_Data()){
        FUNCERR("Attempted to use bounded dose routine, but we do not have contours and/or dose");
    }
    if((pixel_doses != nullptr) && !pixel_doses->empty()){
        FUNCWARN("Requesting to push pixel doses to a non-empty container. Assuming this was intentional");
    }
    if((mean_doses != nullptr) && !mean_doses->empty()){
        FUNCWARN("Requesting to push mean doses to a non-empty container. The vector will be emptied prior to continuing - this is surely a programming error.");
        mean_doses->clear();
    }


    //Determine which ROI numbers we have in the contour data.
    std::set<long int> the_roi_numbers;
    for(auto cc_it = this->contour_data->ccs.begin(); cc_it != this->contour_data->ccs.end(); ++cc_it){
        the_roi_numbers.insert(cc_it->ROI_number);
    }

    //Push back zeros for the output mean_doses so we can += our results to it.
    if(mean_doses != nullptr){
        for(auto it=the_roi_numbers.begin(); it != the_roi_numbers.end(); ++it) (*mean_doses)[*it] = 0.0;
    }

    //This is used to store info about total accumulated dose and total number of voxels within the contours. 
    // It can be used to compute the mean doses.
    // map<ROI number, pair<Total dose (in unscaled integer units), Number of voxels>>.
    std::map<long int, std::pair<int64_t,int64_t>> accumulated_dose;

    //Loop over the attached dose datasets (NOT the dose slices!). It is implied that we have to sum up doses 
    // from each attached data in order to find the total (actual) dose.
    for(auto dd_it = this->dose_data.begin(); dd_it != this->dose_data.end(); ++dd_it){
        //Note: dd_it is something like std::list<std::shared_ptr<Dose_Array>>::iterator.

        //Clear the map/initialize the elements.
        for(auto it = the_roi_numbers.begin(); it != the_roi_numbers.end(); ++it){
            accumulated_dose[*it] = std::pair<int64_t,int64_t>(0,0);
        }

        //We now loop through all dose frames (slices) and accumulate dose within a the contour bounds.
        for(auto i_it = (*dd_it)->imagecoll.images.begin(); i_it != (*dd_it)->imagecoll.images.end(); ++i_it){
            //Note: i_it is something like std::list<planar_image<T,R>>::iterator.
    
            //Loop over the roi numbers we have found. Typically, this will be one single number (ie. maybe it denotes the 'Left Parotid' contours.)
            for(auto r_it = the_roi_numbers.begin(); r_it != the_roi_numbers.end(); ++r_it){
                const long int the_roi_number = *r_it;

                for(auto cc_it = this->contour_data->ccs.begin(); cc_it != this->contour_data->ccs.end(); ++cc_it){
                    if(cc_it->ROI_number != the_roi_number) continue;

                    for(auto c_it = cc_it->contours.begin(); c_it != cc_it->contours.end(); ++c_it){
                        if(c_it->points.size() < 3) continue;

                        const auto filtering_avg_point = c_it->First_N_Point_Avg(3); //Average_Point(); //Just need a point at the correct height, somewhere inside contour.
                        if(!i_it->sandwiches_point_within_top_bottom_planes(filtering_avg_point)) continue;
 
                        //Now we have a contour of points and pixel data (note: we can ignore the z components for both) 
                        // which may or may not lie within the contour. This is called the 'Point-in-polygon' problem and is
                        // a well-known problem. Unfortunately, it can be a costly problem to solve. We will bound the contour with
                        // a cartesian bounding box (in the XY plane) and poll each point. We can immediately ignore points outside the box.
                        // This will save a considerable amount of time for small contours within a large volume, and probably won't greatly
                        // slow the large contour/large volume case too much.
                        //
                        // We then examine line crossings. For a regular polygon, whether or not the number of line crossings (from the point
                        // to infinity (in this case - the edge of the box)) is even or odd will determine whether or not the point is within
                        // the contour. I *believe* there would be an issue if the contour would wrap around and touch exactly on some segment
                        // (like a "C" where the two sharp edges touch to form an "O".) ***BEWARE*** that this is most likely the case!
                        //
                        // See http://www.visibone.com/inpoly/ (Accessed Jan 2012,) 
                        //     http://paulbourke.net/geometry/insidepoly/ (January 2012,)
                        //     http://stackoverflow.com/questions/217578/point-in-polygon-aka-hit-test (January 2012).
                        //     NOTE: Some web servers/web applications used to have to implement this routine to deal with image maps. 
                        //           Check their sources for leads if looking/scrouging for code. I would like a fully 3D version!
                        //
                        //     |---------------------|
                        //     | /-----\  /--------\ |
                        //     | |     |__|        / |
                        //     |/                 /  |
                        //     ||   X            /   |
                        //     | \______________/    |
                        //     |---------------------|
                        //
                        const contour_of_points<double> BB(c_it->Bounding_Box_Along(vec3<double>(1.0,0.0,0.0)));
                        const float alrgnum(1E30);
                        float min_x = alrgnum, max_x = -alrgnum;
                        float min_y = alrgnum, max_y = -alrgnum;
                        for(auto i = BB.points.begin(); i != BB.points.end(); ++i){
                            if(i->x < min_x) min_x = i->x;
                            if(i->x > max_x) max_x = i->x;
                            if(i->y < min_y) min_y = i->y;
                            if(i->y > max_y) max_y = i->y;
                        }
                        if((min_x == alrgnum) || (min_y == alrgnum) || (max_x == -alrgnum) || (max_y == -alrgnum)){
                            FUNCERR("Unable to find a reasonable bounding box around this contour");
                        }
    
                        //Now cycle through every pixel in the plane. This is kind of a shit way to do this, but then again this entire program is basically
                        // a shit way to do it. I would like to have had more time to properly fix/plan/think about what I've got here...  -h
                        for(long int i=0; i<i_it->rows; ++i)  for(long int j=0; j<i_it->columns; ++j){
                            const auto pos = i_it->position(i,j);
                            const float X = pos.x, Y = pos.y;
    
                            //Check if it is outside the bounding box.
                            if(!isininc(min_x,X,max_x) || !isininc(min_y,Y,max_y)) continue;
    
                            bool is_in_the_polygon = false;
                            auto p_j = --(c_it->points.end());
                            for(auto p_i = c_it->points.begin(); p_i != c_it->points.end(); p_j = (p_i++)){
                                //If the points cross a line, we simply toggle the 'bool is_in_the_polygon.'
                                if( ((p_i->y <= Y) && (Y < p_j->y)) || ((p_j->y <= Y) && (Y < p_i->y)) ){ 
                                    const auto B = (p_j->x - p_i->x)*(Y - p_i->y)/(p_j->y - p_i->y);
                                    if(X < (B + p_i->x)){
                                        is_in_the_polygon = !is_in_the_polygon;
                                    }
                                }
                            }
    
                            if(is_in_the_polygon){
                                //-----------------------------------------------------------------------------------------------------------------------
                                //FUNCINFO("Bounding box (x,y)min (x,y)max = (" << min_x << "," << min_y << ") (" << max_x << "," << max_y << ")");
                                //FUNCINFO("Point has (X,Y) = " << X << " " << Y);
    
                                //NOTE: Remember: this is some integer representing dose. If we want a clamped [0:1] 
                                // value, we would use the clamped_channel(...) member instead!
                                const auto pointval = static_cast<int64_t>(i_it->value(i,j,0)); //Greyscale or R channel. We assume the channels satisfy: R = G = B.
     
                                if(mean_doses != nullptr){
                                    accumulated_dose[the_roi_number].first  += pointval;
                                    accumulated_dose[the_roi_number].second += 1;
                                }
    
                                if(pixel_doses != nullptr){ //If we are collecting raw pixel values.
                                    pixel_doses->push_back((*dd_it)->grid_scale * static_cast<double>(pointval)); 
                                }
    
                                //-----------------------------------------------------------------------------------------------------------------------
                            }
                        }
                    }
                }
            }
        }

        //Determine the mean dose if required.
        if(mean_doses != nullptr){
            for(auto it = accumulated_dose.begin(); it != accumulated_dose.end(); it++){
                //If there were no voxels within the contour then we have nothing to do.
                if(it->second.second == 0) continue;

                const auto roinumb = it->first;
                const auto ttldose = static_cast<double>(it->second.first)*(*dd_it)->grid_scale;
                const auto numvxls = static_cast<double>(it->second.second);

                if(ttldose < 0.0) FUNCERR("Total dose was negative (" << ttldose << "). This is not possible");
                if(numvxls < 0.0) FUNCERR("Total number of voxels was negative (" << numvxls << "). This is not possible");

                (*mean_doses)[roinumb] += (ttldose/numvxls); //This dose is now in Gy (cGy?)
            }
        }
    }//Loop over the distinct dose file data.
    return;
}
*/

void Drover::Bounded_Dose_General( std::list<double> *pixel_doses, 
                                   drover_bnded_dose_bulk_doses_map_t *bulk_doses, //NOTE: similar to pixel_doses but not all grouped together...
                                   drover_bnded_dose_mean_dose_map_t *mean_doses, 
                                   drover_bnded_dose_min_max_dose_map_t *min_max_doses,
                                   drover_bnded_dose_pos_dose_map_t *pos_doses,
                                   std::function<bool(bnded_dose_pos_dose_tup_t)> Fselection,
                                   drover_bnded_dose_stat_moments_map_t *cent_moms ) const {
    //This function is a general routine for working with pixels bounded by contour data. It *might* be better to stick it in 
    // the contour or pixel classes, but it seems better (at the moment) to place it in the Drover class, where we have clearly
    // indicated which contour, which dose pixels, and which CT data we want to work with.
    //
    //This function should (probably) only be called internally. Use the wrapper member functions. There is no reason why one
    // couldn't use this function, however, so do whatever you want to!
    //
    //Output options:
    //  std::list<double> *pixel_doses;            <-- Holds the each voxel's dose. Discards spatial info about voxels.
    //  std::map<long int,double> *mean_doses;     <-- Holds the mean dose for each ROI contour number within the contour data.
    //  ....many more implemented...   They should be fairly self-describing...
    //
    // Pass a pointer to the desired container to compute the desired quantities.

    //----------------------------------------- Sanity/Safety Checks ----------------------------------------
    if(!this->Has_Been_Melded){
        FUNCERR("Attempted to use bounded dose routine without melding data. It is required to do so");
    }
    if((pixel_doses == nullptr) && (mean_doses == nullptr) && (min_max_doses == nullptr) 
    && (pos_doses   == nullptr) && (bulk_doses == nullptr) && (cent_moms     == nullptr) ){
        FUNCWARN("No valid output pointers provided. Nothing will be computed");
        return;
    }
    if(!this->Has_Contour_Data() || !this->Has_Dose_Data()){
        FUNCERR("Attempted to use bounded dose routine, but we do not have contours and/or dose");
    }
    if((pixel_doses != nullptr) && !pixel_doses->empty()){
        FUNCWARN("Requesting to push pixel doses to a non-empty container. Assuming this was intentional");
        //This might be intentional if, say, we want to group together lots of pixel doses from several subsegments by
        // picking and choosing (i.e. the hard way - do it the easy way by selecting subsegments into a collection and
        // then collect bulk_doses into nicely-partitioned groups).
    }
    if((mean_doses != nullptr) && !mean_doses->empty()){
        FUNCWARN("Requesting to push mean doses to a non-empty container. Emptying prior to continuing - this is surely a programming error.");
        mean_doses->clear();
    }
    if((bulk_doses != nullptr) && !bulk_doses->empty()){
        FUNCWARN("Requesting to push bulk pixel doses to a non-empty container. Assuming this was intentional");
    }
    if((min_max_doses != nullptr) && !min_max_doses->empty()){
        FUNCWARN("Requesting to push min/max doses to a non-empty container. Emptying prior to continuing - this is surely a programming error.");
        min_max_doses->clear();
    }
    if((pos_doses != nullptr) && !pos_doses->empty()){
        FUNCWARN("Requesting to push positional doses to a non-empty container. Emptying prior to continuing - this is surely a programming error.");
        pos_doses->clear();
    }
    if((pos_doses != nullptr) && !Fselection){
        FUNCERR("Passed space for positional doses but not given a heuristic function (for determining if two points are equal). This is required!");
    }
    if((cent_moms != nullptr) && !cent_moms->empty()){
        FUNCWARN("Requesting centralized moments with a non-empty container. Emptying prior to continuing - we require the working space");
        //Since we have to normalize them at the end, we have to begin with empty space.
    }

    std::list<std::shared_ptr<Dose_Array>> dose_data_to_use(this->dose_data);
    if((min_max_doses != nullptr) && (this->dose_data.size() > 1)){ //Only dose data meld when needed. Moments, for instance, probably don't need to be melded!
/*
        //We need to check if the dose volumes and voxels overlap exactly or not. If they do, we can compute the mean within each
        // separately and sum them afterward. If they don't, the situation becomes very tricky. It is possible to compute it, but
        // it is not done here (if they overlap with zero volume then it is again easy, but it doesn't fit this routine terribly well).
        //
        auto d2_it = this->dose_data.begin(); //Note: d*_it are ~ std::list<std::shared_ptr<Dose_Array>>::iterator
        auto d1_it = --(this->dose_data.end());
        while((d1_it != this->dose_data.end()) && (d2_it != this->dose_data.end())){
            //We will try to compare the layout of the dose volume/array. If everything about them (except the dose values) is 
            // identical, then we can easily handle them (by strictly summing the doses in each voxel).
            if((*d1_it)->imagecoll.Spatially_eq((*d2_it)->imagecoll)){
                d1_it = d2_it;
                ++d2_it;
            }else{
                FUNCINFO("Bits: this: " << (*d1_it)->bits << " and rhs: " << (*d2_it)->bits);
                FUNCINFO("Grid scaling: this: " << (*d1_it)->grid_scale << " and rhs: " << (*d2_it)->grid_scale);
                FUNCERR("This function cannot handle data sets with multiple, distinctly-shaped dose data");
            }
        }
FUNCWARN("This routine will most likely NOT compute valid min/max from multiple dose data. Try collapsing them, if possible, first");
*/
        dose_data_to_use = Meld_Dose_Data(this->dose_data);
        if(dose_data_to_use.size() != 1){
            FUNCERR("This routine cannot handle multiple dose data which cannot be melded. This has " << dose_data_to_use.size());
        }
    }


    //This is currently ONLY used if (cent_moms != nullptr).
    auto cc_centroids = drover_bnded_dose_centroid_map_factory();

    for(auto cc_it = this->contour_data->ccs.begin(); cc_it != this->contour_data->ccs.end(); ++cc_it){
        //Push back zeros for the output mean_doses so we can += our results to it. Probably not necessary.
        if(mean_doses != nullptr) (*mean_doses)[cc_it] = 0.0;

        //Push back impossible values for min/max doses. These will get replaced or will flag an error.
        if(min_max_doses != nullptr) (*min_max_doses)[cc_it] = std::pair<double,double>(1E99, -1E99); //min, max.

        //Pre-compute centroids for each cc. We do this because we end up looping over cc's and they are fairly costly.
        if(cent_moms != nullptr) cc_centroids[cc_it] = cc_it->Centroid();
    }

    //This is used to store info about total accumulated dose and total number of voxels within the contours. 
    // It can be used to compute the mean doses.
    // map<cc_iter, pair<Total dose (in unscaled integer units), Number of voxels>>.
    auto accumulated_dose = drover_bnded_dose_accm_dose_map_factory();

    //Loop over the attached dose datasets (NOT the dose slices!). It is implied that we have to sum up doses 
    // from each attached data in order to find the total (actual) dose.
    //
    //NOTE: Should I get rid of the idea of cycling through multiple dose data? The only way we get here is if the data
    // cannot be melded... This is probably not a worthwhile feature to keep in this code.
    for(auto dd_it = dose_data_to_use.begin(); dd_it != dose_data_to_use.end(); ++dd_it){
//    for(auto dd_it = this->dose_data.begin(); dd_it != this->dose_data.end(); ++dd_it){
        //Note: dd_it is something like std::list<std::shared_ptr<Dose_Array>>::iterator.

        //Clear the accumulation map/initialize the elements.
        for(auto cc_it = this->contour_data->ccs.begin(); cc_it != this->contour_data->ccs.end(); ++cc_it){
            accumulated_dose[cc_it] = std::pair<int64_t,int64_t>(0,0);
        }

        //We now loop through all dose frames (slices) and accumulate dose within the contour bounds.
        for(auto i_it = (*dd_it)->imagecoll.images.begin(); i_it != (*dd_it)->imagecoll.images.end(); ++i_it){
            //Note: i_it is something like std::list<planar_image<T,R>>::iterator.
    
            for(auto cc_it = this->contour_data->ccs.begin(); cc_it != this->contour_data->ccs.end(); ++cc_it){

                for(auto c_it = cc_it->contours.begin(); c_it != cc_it->contours.end(); ++c_it){
                    if(c_it->points.size() < 3) continue;

                    const auto filtering_avg_point = c_it->First_N_Point_Avg(3); //Average_Point(); //Just need a point at the correct height, somewhere inside contour.
                    if(!i_it->sandwiches_point_within_top_bottom_planes(filtering_avg_point)) continue;
 
                    //Now we have a contour of points and pixel data (note: we can ignore the z components for both) 
                    // which may or may not lie within the contour. This is called the 'Point-in-polygon' problem and is
                    // a well-known problem. Unfortunately, it can be a costly problem to solve. We will bound the contour with
                    // a cartesian bounding box (in the XY plane) and poll each point. We can immediately ignore points outside the box.
                    // This will save a considerable amount of time for small contours within a large volume, and probably won't greatly
                    // slow the large contour/large volume case too much.
                    //
                    // We then examine line crossings. For a regular polygon, whether or not the number of line crossings (from the point
                    // to infinity (in this case - the edge of the box)) is even or odd will determine whether or not the point is within
                    // the contour. I *believe* there would be an issue if the contour would wrap around and touch exactly on some segment
                    // (like a "C" where the two sharp edges touch to form an "O".) ***BEWARE*** that this is most likely the case!
                    //
                    // See http://www.visibone.com/inpoly/ (Accessed Jan 2012,) 
                    //     http://paulbourke.net/geometry/insidepoly/ (January 2012,)
                    //     http://stackoverflow.com/questions/217578/point-in-polygon-aka-hit-test (January 2012).
                    //     NOTE: Some web servers/web applications used to have to implement this routine to deal with image maps. 
                    //           Check their sources for leads if looking/scrounging for code. I would like a fully 3D version!
                    //
                    //     |---------------------|
                    //     | /-----\  /--------\ |
                    //     | |     |__|        / |
                    //     |/                 /  |
                    //     ||   X            /   |
                    //     | \______________/    |
                    //     |---------------------|
                    //
                    const contour_of_points<double> BB(c_it->Bounding_Box_Along(vec3<double>(1.0,0.0,0.0)));
                    const float alrgnum(1E30);
                    float min_x = alrgnum, max_x = -alrgnum;
                    float min_y = alrgnum, max_y = -alrgnum;
                    for(auto i = BB.points.begin(); i != BB.points.end(); ++i){
                        if(i->x < min_x) min_x = i->x;
                        if(i->x > max_x) max_x = i->x;
                        if(i->y < min_y) min_y = i->y;
                        if(i->y > max_y) max_y = i->y;
                    }
                    if((min_x == alrgnum) || (min_y == alrgnum) || (max_x == -alrgnum) || (max_y == -alrgnum)){
                        FUNCERR("Unable to find a reasonable bounding box around this contour");
                    }
    
                    //Now cycle through every pixel in the plane. This is kind of a shit way to do this, but then again this entire program is basically
                    // a shit way to do it. I would like to have had more time to properly fix/plan/think about what I've got here...  -h
                    for(long int i=0; i<i_it->rows; ++i)  for(long int j=0; j<i_it->columns; ++j){
                        const auto pos = i_it->position(i,j);
                        const float X = pos.x, Y = pos.y;
    
                        //Check if it is outside the bounding box.
                        if(!isininc(min_x,X,max_x) || !isininc(min_y,Y,max_y)) continue;
    
                        bool is_in_the_polygon = false;
                        auto p_j = --(c_it->points.end());
                        for(auto p_i = c_it->points.begin(); p_i != c_it->points.end(); p_j = (p_i++)){
                            //If the points cross a line, we simply toggle the 'bool is_in_the_polygon.'
                            if( ((p_i->y <= Y) && (Y < p_j->y)) || ((p_j->y <= Y) && (Y < p_i->y)) ){ 
                                const auto B = (p_j->x - p_i->x)*(Y - p_i->y)/(p_j->y - p_i->y);
                                if(X < (B + p_i->x)){
                                    is_in_the_polygon = !is_in_the_polygon;
                                }
                            }
                        }

                        if(is_in_the_polygon){
                            //-----------------------------------------------------------------------------------------------------------------------
                            //FUNCINFO("Bounding box (x,y)min (x,y)max = (" << min_x << "," << min_y << ") (" << max_x << "," << max_y << ")");
                            //FUNCINFO("Point has (X,Y) = " << X << " " << Y);

                            //NOTE: Remember: this is some integer representing dose. If we want a clamped [0:1] 
                            // value, we would use the clamped_channel(...) member instead!
                            const auto pointval = static_cast<int64_t>(i_it->value(i,j,0)); //Greyscale or R channel. We assume the channels satisfy: R = G = B.
                            const auto pointdose = (*dd_it)->grid_scale * static_cast<double>(pointval); 

                            if(mean_doses != nullptr){
                                accumulated_dose[cc_it].first  += pointval;
                                accumulated_dose[cc_it].second += 1;
                            }
                            if(bulk_doses != nullptr){
                                (*bulk_doses)[cc_it].push_back(pointdose);
                            }

                            if(pixel_doses != nullptr){
                                pixel_doses->push_back(pointdose); 
                            }

                            if(min_max_doses != nullptr){
                                if(pointdose < (*min_max_doses)[cc_it].first)  (*min_max_doses)[cc_it].first  = pointdose; //min.
                                if(pointdose > (*min_max_doses)[cc_it].second) (*min_max_doses)[cc_it].second = pointdose; //max.
                            }

                            if(pos_doses != nullptr){
                                const vec3<double> r_dx = i_it->row_unit*i_it->pxl_dx*0.5;
                                const vec3<double> r_dy = i_it->col_unit*i_it->pxl_dy*0.5;
                                const auto tup = std::make_tuple(pos, r_dx, r_dy, pointdose, i, j);

                                if(Fselection(tup)) (*pos_doses)[cc_it].push_back(std::move(tup));
                            }
                            if(cent_moms != nullptr){ //Centralized moments. This routine requires a centroid for each cc.
                                const auto cc_centroid = cc_centroids[cc_it];
                                for(int p = 0; p < 5; ++p) for(int q = 0; q < 5; ++q) for(int r = 0; r < 5; ++r){
                                    //const std::array<int,3> triplet = {p,q,r};
                                    const auto spatial = pow(pos.x-cc_centroid.x,p)*pow(pos.y-cc_centroid.y,q)*pow(pos.z-cc_centroid.z,r);
                                    const auto grid_factor = i_it->pxl_dx * i_it->pxl_dy * i_it->pxl_dz;
                                    (*cent_moms)[cc_it][{p,q,r}] += spatial*pointdose*grid_factor;
                                }
                            }

                            //-----------------------------------------------------------------------------------------------------------------------
                        }
                    }
                }
            }
        }

        //Determine the mean dose if required.
        if(mean_doses != nullptr){
            for(auto it = accumulated_dose.begin(); it != accumulated_dose.end(); it++){
                //If there were no voxels within the contour then we have nothing to do.
                if(it->second.second == 0) continue;

                const auto cc_iter = it->first;
                const auto ttldose = static_cast<double>(it->second.first)*(*dd_it)->grid_scale;
                const auto numvxls = static_cast<double>(it->second.second);

                if(ttldose < 0.0) FUNCERR("Total dose was negative (" << ttldose << "). This is not possible");
                if(numvxls < 0.0) FUNCERR("Total number of voxels was negative (" << numvxls << "). This is not possible");

                (*mean_doses)[cc_iter] += (ttldose/numvxls); //This dose is now in Gy (cGy?)
            }
        }
    }//Loop over the distinct dose file data.


    //Verification.
    if(min_max_doses != nullptr){
         for(auto it = min_max_doses->begin(); it != min_max_doses->end(); ++it){
             const auto min = it->second.first, max = it->second.second;
             if(min > max){
                 //If there was no dose present, this is not an error.
                 const auto cc_iter = it->first;
                 if((mean_doses != nullptr) && (mean_doses->find(cc_iter) != mean_doses->end()) && ((*mean_doses)[cc_iter] == 0.0)){
                     it->second.first  = 0.0;
                     it->second.second = 0.0;

                 //Otherwise, we don't know if this is an error or not. Issue a warning but do not 
                 // adjust the values.
                 }else{
                     FUNCWARN("Contradictory min = " << min << " and max = " << max);
                 }
             }
         }
    }
    return;
}

std::list<double> Drover::Bounded_Dose_Bulk_Values(void) const {
    std::list<double> outgoing;
    this->Bounded_Dose_General(&outgoing,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);
    return outgoing;
}

drover_bnded_dose_mean_dose_map_t Drover::Bounded_Dose_Means(void) const {
    //NOTE: This function returns a map from list<contours_with_meta>::iterators to doubles representing the mean doses.
    //      Please be careful to ensure that the iterators are not invalidated between calling this and reading the values.
    //      
    //      If you need some specific quantities (segmentation history, roi number, etc..) it is safer to store those things
    //      ASAP after calling this and then discarding the given map.
    //
    auto outgoing = drover_bnded_dose_mean_dose_map_factory();
    this->Bounded_Dose_General(nullptr,nullptr,&outgoing,nullptr,nullptr,nullptr,nullptr);
    return std::move(outgoing);
}

drover_bnded_dose_min_max_dose_map_t Drover::Bounded_Dose_Min_Max(void) const {
    //NOTE: See note in Drover::Bounded_Dose_Means() regarding invalidation of this map.
    //
    //NOTE: The map returns a pair. The first is min, the second is max.
    auto outgoing = drover_bnded_dose_min_max_dose_map_factory();
    this->Bounded_Dose_General(nullptr,nullptr,nullptr,&outgoing,nullptr,nullptr,nullptr);
    return std::move(outgoing);
}

drover_bnded_dose_min_mean_max_dose_map_t Drover::Bounded_Dose_Min_Mean_Max(void) const {
    //NOTE: See note in Drover::Bounded_Dose_Means() regarding invalidation of this map.
    auto outgoing = drover_bnded_dose_min_mean_max_dose_map_factory();

    auto means   = drover_bnded_dose_mean_dose_map_factory();
    auto minmaxs = drover_bnded_dose_min_max_dose_map_factory();
    this->Bounded_Dose_General(nullptr,nullptr,&means,&minmaxs,nullptr,nullptr,nullptr);

    if(means.size() != minmaxs.size()){
        FUNCERR("Number of means did not match number of min/maxs. Must have encountered a computational error");
    }

    for(auto it = means.begin(); it != means.end(); ++it){
        const auto theiter = it->first;
        const auto min    = minmaxs[theiter].first;
        const auto mean   = it->second;
        const auto max    = minmaxs[theiter].second;
        outgoing[theiter] = std::make_tuple(min, mean, max); 
    }
    return std::move(outgoing);
}

drover_bnded_dose_min_mean_median_max_dose_map_t Drover::Bounded_Dose_Min_Mean_Median_Max(void) const {
    //NOTE: See note in Drover::Bounded_Dose_Means() regarding invalidation of this map.
    auto outgoing = drover_bnded_dose_min_mean_median_max_dose_map_factory();

    auto means   = drover_bnded_dose_mean_dose_map_factory();
    auto minmaxs = drover_bnded_dose_min_max_dose_map_factory();
    auto bulks   = drover_bnded_dose_bulk_doses_map_factory();
    this->Bounded_Dose_General(nullptr,&bulks,&means,&minmaxs,nullptr,nullptr,nullptr);

    if(means.size() != minmaxs.size()){
        FUNCERR("Number of means did not match number of min/maxs. Must have encountered a computational error");
    }

    for(auto it = means.begin(); it != means.end(); ++it){
        const auto theiter = it->first;
        const auto min    = minmaxs[theiter].first;
        const auto mean   = it->second;
        const auto median = Stats::Median(bulks[theiter]);
        const auto max    = minmaxs[theiter].second;
        outgoing[theiter] = std::make_tuple(min, mean, median, max);
    }
    return std::move(outgoing);
}

drover_bnded_dose_stat_moments_map_t Drover::Bounded_Dose_Centralized_Moments(void) const {
    auto outgoing = drover_bnded_dose_stat_moments_map_factory();
    this->Bounded_Dose_General(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&outgoing);
    return std::move(outgoing);
}

drover_bnded_dose_stat_moments_map_t Drover::Bounded_Dose_Normalized_Cent_Moments(void) const {
    auto outgoing = this->Bounded_Dose_Centralized_Moments();
 
    //We normalize moments with respect to the p,q,r=0,0,0 moment for the given cc.
    // We also REMOVE those for which normalization is not useful or defined.
    for(auto m_it = outgoing.begin(); m_it != outgoing.end(); ++m_it){
        //auto cc_it = m_it->first;
        //second is also a map --->  std::map<std::array<int,3>,double>.
        const auto m000 = m_it->second[{0,0,0}];
        if(m000 == 0.0) FUNCERR("Cannot normalize - m000 is zero. Unable to continue");

        auto mm_it = m_it->second.begin();
        while(mm_it != m_it->second.end()){
            const auto p = mm_it->first[0], q = mm_it->first[1], r = mm_it->first[2];
            if((p+q+r) > 1){
                mm_it->second /= pow(m000, 1.0 + static_cast<double>(p+q+r)/3.0);
                ++mm_it;
            }else{
                mm_it = m_it->second.erase(mm_it);
            }
        }
    }

    return std::move(outgoing);
}


Drover Drover::Segment_Contours_Heuristically(std::function<bool(bnded_dose_pos_dose_tup_t)> Fselection) const {
    Drover out(*this);
    if(!out.Meld(false)) FUNCERR("Unable to meld data");
 
    if(!Fselection) FUNCERR("Passed an unaccessible heuristic function. Unable to continue");

    //First, get the positional dose data (using the copy).
    auto pos_dose = drover_bnded_dose_pos_dose_map_factory();
    out.Bounded_Dose_General(nullptr,nullptr,nullptr,nullptr,&pos_dose,Fselection,nullptr);

    //Now clear the existing contours in the copy, leaving empty collections
    // and valid iterators to them.
    for(auto cc_it = out.contour_data->ccs.begin(); cc_it != out.contour_data->ccs.end(); ++cc_it){
        cc_it->Segmentation_History.push_back(Segmentations::misc_marker);
        cc_it->contours.clear();
    }


    for(auto m_it = pos_dose.begin(); m_it != pos_dose.end(); ++m_it){
        auto cc_it   = m_it->first;  //std::list<contours_with_meta>::iterator == bnded_dose_map_key_t.
        auto thelist = m_it->second; //std::list<std::tuple<..,..,..,...>> 

        //Cycle through the points (ie. centre of voxels) sorting them into heights.
        std::map<double, std::list<bnded_dose_pos_dose_tup_t>> levels;            //<--- use a std::set so we can auto-sort on indices?
        vec3<double> r_dx, r_dy;
        for(auto l_it = thelist.begin(); l_it != thelist.end(); ++l_it){
            const auto pos = std::get<0>(*l_it);
            r_dx = std::get<1>(*l_it);
            r_dy = std::get<2>(*l_it);
            const auto unit_z = r_dx.Cross(r_dy).unit();
            const auto z = pos.Dot(unit_z);
            levels[z].push_back(std::move(*l_it));
        }
        thelist.clear();

        //Using some token r_dx and r_dy values, construct a reasonable 'points are equal' closure.
        // This should be fairly easy because everything is on a grid (from the dose grid).
        const double dx_sep_thres = 0.4*r_dx.length();
        const double dy_sep_thres = 0.4*r_dy.length();
        const double dz_sep_thres = 0.4*(( cc_it->Minimum_Separation <= 0.0 ) ? r_dx.length() : cc_it->Minimum_Separation);
        const auto points_are_equal = [=](const vec3<double> &A, const vec3<double> &B) -> bool {
            const auto C = A-B;
            return (YGORABS(C.x) < dx_sep_thres) && (YGORABS(C.y) < dy_sep_thres) && (YGORABS(C.z) < dz_sep_thres);
        };

        //Now run over each level, create contours as per the specifications of each point,
        // and attempt to merge them. Some will not merge - this is OK.
        // Push them into the outgoing contour data. No other merging is possible because no
        // other points are on this level.
        for(auto lm_it = levels.begin(); lm_it != levels.end();    ){ 
            contour_collection<double> stage;
            for(auto l_it = lm_it->second.begin(); l_it != lm_it->second.end(); ++l_it){
                const auto pos  = std::get<0>(*l_it);
                r_dx = std::get<1>(*l_it);
                r_dy = std::get<2>(*l_it);

                contour_of_points<double> shtl;
                shtl.closed = true;
                shtl.points.push_back( pos + r_dx + r_dy ); //Ensure they are oriented identically for merging!
                shtl.points.push_back( pos - r_dx + r_dy );
                shtl.points.push_back( pos - r_dx - r_dy );
                shtl.points.push_back( pos + r_dx - r_dy );
                stage.contours.push_back( std::move(shtl) );
            }
            lm_it = levels.erase(lm_it);

            stage.Merge_Adjoining_Contours(points_are_equal);
            for(auto c_it = stage.contours.begin(); c_it != stage.contours.end(); ++c_it){
                c_it->Remove_Extraneous_Points(points_are_equal);
                cc_it->contours.push_back( std::move(*c_it) );
            }
        }
    }

    return std::move(out);
}

std::pair<double,double> Drover::Bounded_Dose_Limits(void) const {
    std::list<double> doses;
    this->Bounded_Dose_General(&doses,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);
    if(doses.empty()) return std::pair<double,double>(-1.0,-1.0);

    return std::pair<double,double>(*std::min_element(doses.begin(),doses.end()), *std::max_element(doses.begin(),doses.end()));
}

std::map<double,double>  Drover::Get_DVH(void) const {
    std::map<double,double> output;

    if( this->Has_Been_Melded != true ){
        FUNCWARN("Attempted to compute a DVH prior to melding data. Ensure Drover::Meld(); is called *prior* to computing anything using the data!");
        return output;  //We need to meld first!
    }

    std::list<double> pixel_doses = this->Bounded_Dose_Bulk_Values();
    if(pixel_doses.empty()){
        //FUNCERR("Unable to compute DVH: There was no data in the pixel_doses structure!");
        FUNCWARN("Asked to compute DVH when no voxels appear to have any dose. This is physically possible, but please be sure it is what you expected");
        //Could be due to:
        // -contours being too small (much smaller than voxel size).
        // -dose and contours not aligning properly. Maybe due to incorrect offsets/rotations/coordinate system?
        // -dose/contours not being present. Maybe accidentally?
        output[0.0] = 0.0; //Nothing over 0Gy is delivered to any voxel (0% of volume).
        return std::move(output);
    }

    double cumulative;
    double test_dose = 0.0;
    do{
        cumulative = 0.0;
        for(auto i = pixel_doses.begin(); i != pixel_doses.end(); ++i){
            if(*i > test_dose) cumulative += 1.0;
        }

        const auto dose = test_dose;
        const auto frac = static_cast<double>(cumulative) / static_cast<double>(pixel_doses.size());
        output[dose] = frac;
        test_dose += 0.5;
    }while(cumulative != 0.0);
    return std::move(output);
}

Drover Drover::Duplicate(std::shared_ptr<Contour_Data> in) const {
    Drover output(*this);     //Copy self.
    output.contour_data = in; //Place the new contour data in the clone.
    return output;
}

Drover Drover::Duplicate(const Drover &in) const {
    return Drover(in);
}


bool Drover::Is_Melded(void) const {
    return this->Has_Been_Melded;
}

bool Drover::Meld(bool VERBOSITY){
    //This function attempts to identify mismatched data. It looks for data which is not positioned
    // in the same region, or data which does not 'smell' like it should fit together.
    //
    //Although UID's and patient numbers will handle the brunt of finding mismatched data from 
    // separate patients, the intent of this function is to ensure that WE are able to correctly
    // work with the data. In other words, if we have made a mistake parsing the data, this code
    // tries to identify that fact. It might save us from producing non-sensical outputs.
    //
    //This function returns 'true' if no errors are suspected, 'false' if any are.
    // A verbosity parameter is used to explicitly state if any (non-error) oddities are encountered.
    this->Has_Been_Melded = true;


    //------------------------------------ contour checks ----------------------------------------
    if(!this->Has_Contour_Data()){
        if(VERBOSITY) FUNCINFO("No contour data attached");
    }else{

        //If there are multiple sets, check them against each other.

        // ...


    }


    //-------------------------------------- dose checks -----------------------------------------
    if(!this->Has_Dose_Data()){
        if(VERBOSITY) FUNCINFO("No dose data attached");
    }else{

        //If there are multiple sets, check them against each other.

        // ...


    //NOTE: Dose_Data_Meld now performs an explicit dose meld. It is in a separate file. It is not
    // perfect or complete, so it should be finished and augmented with simple melding (here?)


    }

    //-------------------------------------- image checks ----------------------------------------
    if(!this->Has_Image_Data()){
        if(VERBOSITY) FUNCINFO("No image data attached");
    }else{

        //If there are multiple sets, check them against each other.

        // ...


    }


    //-------------------------------------- other checks ----------------------------------------

    // ...

    return true;
}


bool Drover::Has_Contour_Data(void) const {
    return (this->contour_data != nullptr);
}

bool Drover::Has_Dose_Data(void) const {
    //Does not verify the image data itself, it merely looks to see if we have any valid Dose_Arrays attached.
    if(this->dose_data.size() == 0) return false;
    for(auto dd_it=this->dose_data.begin(); dd_it != this->dose_data.end(); ++dd_it){
        if((*dd_it) == nullptr ) return false;
    }
    return true;
}

bool Drover::Has_Image_Data(void) const {
    //Does not verify the image data itself, it merely looks to see if we have any valid Image_Arrays attached.
    if(this->image_data.size() == 0) return false;
    for(auto id_it=this->image_data.begin(); id_it != this->image_data.end(); ++id_it){
        if((*id_it) == nullptr ) return false; 
    }
    return true;
}


void Drover::Concatenate(std::shared_ptr<Contour_Data> in){
    //If there are no existing contours, incoming contours are shared instead of copied.
    // Otherwise, incoming contours are copied and concatenated into *this' contour_data.
    if(in == nullptr) return;
    if(this->contour_data == nullptr){
        this->contour_data = in;
        return;
    }

    auto dup = in->Duplicate();
    this->contour_data->ccs.splice( this->contour_data->ccs.end(),
                                    std::move(dup->ccs) );
    return;
}

void Drover::Concatenate(std::list<std::shared_ptr<Dose_Array>> in){
    this->dose_data.splice( this->dose_data.end(), in );
    return;
}

void Drover::Concatenate(std::list<std::shared_ptr<Image_Array>> in){
    this->image_data.splice( this->image_data.end(), in );
    return;
}

void Drover::Concatenate(Drover in){
    this->Concatenate(in.contour_data);
    this->Concatenate(in.dose_data);
    this->Concatenate(in.image_data);
    return;
}

void Drover::Consume(std::shared_ptr<Contour_Data> in){
    //Consumes incoming contours, moving them from the input (which might be shared) and concatenates
    // them into *this' contour_data. (Ignore the shared_ptr -- contours are consumed!)
    //
    // NOTE: Only use this routine if you:
    //         (1) are OK with yanking the shared contour data from other owners, or
    //         (2) you need to avoid copying/duplicating the contours.
    //       Typically, you would only need to use this routine when iteratively building a large
    //       contour collection from a variety of sources.
    //
    if(in == nullptr) return;
    if(this->contour_data == nullptr){
        this->contour_data = std::make_shared<Contour_Data>();
    }
    this->contour_data->ccs.splice( this->contour_data->ccs.end(),
                                    std::move(in->ccs) );
    in->ccs.clear();
    return;
}

void Drover::Consume(std::list<std::shared_ptr<Dose_Array>> in){
    this->Concatenate(in);
    return;
}

void Drover::Consume(std::list<std::shared_ptr<Image_Array>> in){
    this->Concatenate(in);
    return;
}

void Drover::Consume(Drover in){
    this->Concatenate(in);
    return;
}



void Drover::Plot_Dose_And_Contours(void) const {
    //The aim of this program is to plot contours and dose in the same display. It is probably best
    // for debugging. Use the overlaydosedata program to display data using OpenGL.
    Plotter3 a_plot;
    a_plot.Set_Global_Title("Dose and Contours.");//: Object with address " + Xtostring<long int>((size_t)((void *)(this))));
    vec3<double> r;

    if(this->Has_Dose_Data()){
        for(auto l_it = this->dose_data.begin(); l_it != this->dose_data.end(); ++l_it){  //std::list<std::shared_ptr<Dose_Array>>.
            for(auto pi_it = (*l_it)->imagecoll.images.begin(); pi_it != (*l_it)->imagecoll.images.end(); ++pi_it){ //std::list<planar_image<T,R>> images.
                //Plot the corners in a closed contour to show the outline.
                r = pi_it->position(             0,                0);    a_plot.Insert(r.x,r.y,r.z);
                r = pi_it->position( pi_it->rows-1,                0);    a_plot.Insert(r.x,r.y,r.z);
                r = pi_it->position( pi_it->rows-1, pi_it->columns-1);    a_plot.Insert(r.x,r.y,r.z);
                r = pi_it->position(             0, pi_it->columns-1);    a_plot.Insert(r.x,r.y,r.z);
                r = pi_it->position(             0,                0);    a_plot.Insert(r.x,r.y,r.z); //Close the outline.
                a_plot.Next_Line_Same_Style();
            }
        }
        a_plot.Next_Line();
    }

    if(this->Has_Contour_Data()){
        for(auto cc_it = this->contour_data->ccs.begin(); cc_it != this->contour_data->ccs.end(); ++cc_it){ 
            for(auto c_it = cc_it->contours.begin(); c_it != cc_it->contours.end(); ++cc_it){
                for(auto p_it = c_it->points.begin(); p_it != c_it->points.end(); ++p_it){
                    a_plot.Insert(p_it->x, p_it->y, p_it->z);
                }
                a_plot.Next_Line_Same_Style();
            }
            a_plot.Next_Line();
        }
    }
    a_plot.Plot();
    return;
}

void Drover::Plot_Image_Outlines(void) const {
    Plotter3 a_plot;
    a_plot.Set_Global_Title("Dose and Contours.");//: Object with address " + Xtostring<long int>((size_t)((void *)(this))));
    vec3<double> r;

    if(this->Has_Image_Data()){
        for(const auto &pic_ptr : this->image_data){
            for(const auto &img : pic_ptr->imagecoll.images){
                r = img.position(          0,             0);    a_plot.Insert(r.x,r.y,r.z);
                r = img.position( img.rows-1,             0);    a_plot.Insert(r.x,r.y,r.z);
                r = img.position( img.rows-1, img.columns-1);    a_plot.Insert(r.x,r.y,r.z);
                r = img.position(          0, img.columns-1);    a_plot.Insert(r.x,r.y,r.z);
                r = img.position(          0,             0);    a_plot.Insert(r.x,r.y,r.z); //Close the outline.
                a_plot.Next_Line_Same_Style();
            }
        }
        a_plot.Next_Line();
    }
    a_plot.Plot();
    return;
}


//---------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------ OperationArgPkg ----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------

OperationArgPkg::OperationArgPkg(std::string unparsed, std::string sepr, std::string eqls) : opts(icase_str_lt_lambda) {
    //Parse the string. Throws if an error is detected.
    //
    // Note: for the following examples 'sepr' is ":" and 'eqls' is "=".
    //
    // Examples of acceptable input:
    //   1. "OperationName:keyA=valueA:keyB=valueB"
    //   2. "op_name"
    //   3. "op_name:"
    //   4. "op_name:somelongkey=somelongvalue"
    //   5. "op name:some long key=some long value"
    //   6. "op_name:keyA=:keyB=something"
    //   7. "op_name:keyA=:keyB=something"
    //   8. "  op  name:"
    // etc..
    //
    // Unacceptable:
    //   1. ":op_name"
    //   1. ":keyA=valA"
    //   2. "opname:key_with_no_value:"
    //   3. "opname:key_with_no_value"
    // etc..
    //
    // Spaces are aggressively trimmed. No spaces will be retained at the front or back of keys or values.
    // All sequential are trimmed to a single space.

    unparsed = boost::algorithm::trim_all_copy(unparsed);
    if(unparsed.empty()) throw std::invalid_argument("No operation name specified.");

    if(!boost::algorithm::contains(unparsed, sepr)){
        this->name = unparsed;
        return;
    }

    std::list<std::string> split_on_colons;
    boost::algorithm::split(split_on_colons, unparsed, boost::algorithm::is_any_of(sepr), boost::algorithm::token_compress_on);
    for(auto &b : split_on_colons) b = boost::algorithm::trim_all_copy(b);
    if(split_on_colons.empty()) throw std::invalid_argument("No operation name specified.");

    this->name = boost::algorithm::trim_all_copy(split_on_colons.front());
    split_on_colons.pop_front();

    for(auto &a : split_on_colons){
        //Must contain an '=' to be considered valid.
        a = boost::algorithm::trim_all_copy(a);
        if(a.empty()) continue;
        if(!boost::algorithm::contains(a, eqls)) throw std::invalid_argument("Argument provided with key but no value");

        std::list<std::string> split_on_equals;
        boost::algorithm::split(split_on_equals, a, boost::algorithm::is_any_of(eqls), boost::algorithm::token_compress_off);
        for(auto &b : split_on_equals) b = boost::algorithm::trim_all_copy(b);

        if(split_on_equals.size() != 2) throw std::invalid_argument("Missing argument key or value");
        if(0 != this->opts.count(split_on_equals.front())){
            throw std::invalid_argument("Provided argument would overwrite existing argument");
        }
        if(split_on_equals.front().empty()){
            throw std::invalid_argument("Unwilling to create empty argument key");
        }

        this->opts[split_on_equals.front()] = split_on_equals.back();
        
    }
    return;
}

OperationArgPkg &
OperationArgPkg::operator=(const OperationArgPkg &rhs){
    if(this == &rhs) return *this;
    this->name = rhs.name;
    this->opts = rhs.opts;
    return *this;
}

std::string
OperationArgPkg::getName(void) const {
    return this->name;
}


bool 
OperationArgPkg::containsExactly(std::initializer_list<std::string> l) const {
   //We compare the number of elements, scan elements of setA to see if they're in setB, and then vice-versa.
   //
   // Note: Case sensitivity in this functions' argument can cause issues. Don't send in, e.g., 'foo' and 'FOO' 
   //       and expect this routine to work!
   //
   if(l.size() != this->opts.size()) return false;
   for(const auto &i : l){
       bool wasPresent = false;
       for(const auto &kv : this->opts){
           if(boost::algorithm::iequals(kv.first,i)){
               wasPresent = true;
               break;
           }
       }
       if(!wasPresent) return false;
   }
   for(const auto &kv : this->opts){
       bool wasPresent = false;
       for(const auto &i : l){
           if(boost::algorithm::iequals(kv.first,i)){
               wasPresent = true;
               break;
           }
       }
       if(!wasPresent) return false;
   }
   return true;
}


//Returns value corresponding to key. Optional is disengaged if key is missing or cast fails.
std::experimental::optional<std::string> 
OperationArgPkg::getValueStr(std::string key) const {
    const auto cit = this->opts.find(key);
    auto def = std::experimental::optional<std::string>();

    if(cit == this->opts.end()){
        return std::experimental::optional<std::string>();
    }
    return std::experimental::make_optional(cit->second);
}


//Will not overwrite.
bool
OperationArgPkg::insert(std::string key, std::string val){
    if(0 != this->opts.count(key)){
        return false;
    }
    this->opts[key] = val;
    return true;
}


