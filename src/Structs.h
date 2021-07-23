//Structs.h.

#pragma once

#include <array>
#include <optional>
#include <initializer_list>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <variant>
#include <any>

#include <YgorMisc.h>
#include "YgorImages.h"
#include "YgorMath.h"

#include "Alignment_TPSRPM.h"


//This is a wrapper around the YgorMath.h class "contour_of_points." It holds an instance of a contour_of_points, but also provides some meta information
// which helps identify the origin, quality, and purpose of the data.
//
//This should be turned into an enum, I think. Or at least reordered numerically.
namespace Segmentations {
    //General orientations.
    const uint32_t left              = (uint32_t)(1) <<  0; //1;
    const uint32_t right             = (uint32_t)(1) <<  1; //2;
    const uint32_t front             = (uint32_t)(1) <<  2; //4;
    const uint32_t back              = (uint32_t)(1) <<  3; //8;
    const uint32_t top               = (uint32_t)(1) <<  4; //16; //aka "up"
    const uint32_t bottom            = (uint32_t)(1) <<  5; //32; //aka "down"
    const uint32_t other_orientation = (uint32_t)(1) <<  6; //64;
    const uint32_t inner             = (uint32_t)(1) << 20; //1048576;
    const uint32_t outer             = (uint32_t)(1) << 21; //2097152;

    //Placeholder orientations - probably not useful/meaningful except for temporary uses.
    const uint32_t negative          = (uint32_t)(1) <<  7; //128; //This contour was below (on the positive side of) the plane we split on.
    const uint32_t positive          = (uint32_t)(1) <<  8; //256; //This contour was above (on the negative side of) the plane we split on.

    //Splitting type.
    const uint32_t volume            = (uint32_t)(1) <<  9; //512;   //Splitting on an entire volume of contours.
    const uint32_t height            = (uint32_t)(1) << 10; //1024;  //Splitting on all the contours of a given type at the same height.
    const uint32_t contour           = (uint32_t)(1) << 11; //2048;  //Splitting on individual contours.
    const uint32_t other_split       = (uint32_t)(1) << 12; //4096;

    //Splitting direction.
    const uint32_t coronal           = (uint32_t)(1) << 13; //8192;
    const uint32_t transverse        = (uint32_t)(1) << 23; //8388608;

    const uint32_t medial            = (uint32_t)(1) << 14; //16384;
    const uint32_t sagittal          = (uint32_t)(1) << 15; //32768;
    const uint32_t lateral           = (uint32_t)(1) << 16; //65536;
    const uint32_t ant_post          = (uint32_t)(1) << 17; //131072;

    //Other things.
    const uint32_t ray_cast          = (uint32_t)(1) << 18; //262144;  //Default is planar split.
    const uint32_t misc_marker       = (uint32_t)(1) << 19; //524288;
    const uint32_t core_peel         = (uint32_t)(1) << 22; //4194304;
    //Note: std::numeric_limits<uint32_t>::max() == 4294967295.
}

std::string Segmentations_to_Words(const std::vector<uint32_t> &in);


//This class is used to hold a collection of contours.
class Contour_Data {
    public:
        std::list<contour_collection<double>> ccs; //Contour collections.

        //Constructors.
        Contour_Data();
        Contour_Data(const Contour_Data &in);

        //Member functions.
        void operator = (const Contour_Data &rhs);

        void Plot() const;     //Spits out a default plot of the (entirety) of the data. Use the contour_of_points::Plot() method for individual contours.
    
        //Unique duplication (aka 'copy factory').
        std::unique_ptr<Contour_Data> Duplicate() const; 

        //--- Geometeric splitting. ---
        //For each distinct ROI number, we specify a single plane to split on.
        std::unique_ptr<Contour_Data> Split_Per_Volume_Along_Given_Plane_Unit_Normal(const vec3<double> &N) const; 
        std::unique_ptr<Contour_Data> Split_Per_Volume_Along_Coronal_Plane() const; 
        std::unique_ptr<Contour_Data> Split_Per_Volume_Along_Sagittal_Plane() const;

std::unique_ptr<Contour_Data> Split_Per_Volume_Along_Transverse_Plane() const;

        //For each distinct ROI number, distinct height, we specify a plane to split on.
//        std::unique_ptr<Contour_Data> Split_Per_Height_Along_Given_Plane_Unit_Normal(const vec3<double> &N) const;
//        std::unique_ptr<Contour_Data> Split_Per_Height_Along_Coronal_Plane(void) const; 
//        std::unique_ptr<Contour_Data> Split_Per_Height_Along_Sagittal_Plane(void) const;

        //For each distinct contour, we specify a plane to split on.
        std::unique_ptr<Contour_Data> Split_Per_Contour_Along_Given_Plane_Unit_Normal(const vec3<double> &N) const;
        std::unique_ptr<Contour_Data> Split_Per_Contour_Along_Coronal_Plane() const; 
        std::unique_ptr<Contour_Data> Split_Per_Contour_Along_Sagittal_Plane() const;
 
        //--- Ray-casting splitting. ---
        std::unique_ptr<Contour_Data> Raycast_Split_Per_Contour_Against_Given_Direction(const vec3<double> &U) const;
        std::unique_ptr<Contour_Data> Raycast_Split_Per_Contour_Into_ANT_POST() const;
        std::unique_ptr<Contour_Data> Raycast_Split_Per_Contour_Into_Lateral() const;

        //--- Core-Peel splitting. ---
        std::unique_ptr<Contour_Data> Split_Core_and_Peel(double frac_dist) const;

};


class Image_Array { //: public Base_Array {
    public:

        planar_image_collection<float,double> imagecoll;

        std::string filename; //The filename from which the data originated, if applicable.

        //Constructor/Destructors.
        Image_Array();
        Image_Array(const Image_Array &rhs); //Performs a deep copy (unless copying self).

        //Member functions.
        Image_Array & operator=(const Image_Array &rhs); //Performs a deep copy (unless copying self).
};


// This class is meant to hold a simple 3D point cloud.
class Point_Cloud {
    public:

        point_set<double> pset;

        //Constructor/Destructors.
        Point_Cloud();
        Point_Cloud(const Point_Cloud &rhs); //Performs a deep copy (unless copying self).

        //Member functions.
        Point_Cloud & operator=(const Point_Cloud &rhs); //Performs a deep copy (unless copying self).
};


// This class is meant to hold multiple surface meshes that represent a single logical object.
class Surface_Mesh {
    public:

        fv_surface_mesh<double, uint64_t> meshes;

        // Used for defining attributes at run-time.
        // The std::any can be replaced by something like std::map<size_t, double> but doesn't have to.
        std::map< std::string, std::any > vertex_attributes; 
        std::map< std::string, std::any > face_attributes; 

        //Constructor/Destructors.
        Surface_Mesh();
        Surface_Mesh(const Surface_Mesh &rhs); //Performs a deep copy (unless copying self).

        //Member functions.
        Surface_Mesh & operator=(const Surface_Mesh &rhs); //Performs a deep copy (unless copying self).
};


// The class represents a static, instantaneous snapshot of a radiotherapy treatment machine.
// It represents a 'control point' or 'keyframe' for a dynamic particle beam trajectory in state space that can be
// interpolated against another control point to provide a dynamic view.
//
// Note: NaNs in member variables can either denote the data is not applicable, or has not changed since the last time
//       it was specified. 
class Static_Machine_State {
    public:

        double CumulativeMetersetWeight             = std::numeric_limits<double>::quiet_NaN();
        long int ControlPointIndex                  = std::numeric_limits<long int>::min();

        double GantryAngle                          = std::numeric_limits<double>::quiet_NaN();
        double GantryRotationDirection              = std::numeric_limits<double>::quiet_NaN();

        double BeamLimitingDeviceAngle              = std::numeric_limits<double>::quiet_NaN();
        double BeamLimitingDeviceRotationDirection  = std::numeric_limits<double>::quiet_NaN();

        double PatientSupportAngle                  = std::numeric_limits<double>::quiet_NaN();
        double PatientSupportRotationDirection      = std::numeric_limits<double>::quiet_NaN();

        double TableTopEccentricAngle               = std::numeric_limits<double>::quiet_NaN();
        double TableTopEccentricRotationDirection   = std::numeric_limits<double>::quiet_NaN();

        double TableTopVerticalPosition             = std::numeric_limits<double>::quiet_NaN();
        double TableTopLongitudinalPosition         = std::numeric_limits<double>::quiet_NaN();
        double TableTopLateralPosition              = std::numeric_limits<double>::quiet_NaN();

        double TableTopPitchAngle                   = std::numeric_limits<double>::quiet_NaN();
        double TableTopPitchRotationDirection       = std::numeric_limits<double>::quiet_NaN();

        double TableTopRollAngle                    = std::numeric_limits<double>::quiet_NaN();
        double TableTopRollRotationDirection        = std::numeric_limits<double>::quiet_NaN();

        vec3<double> IsocentrePosition              = vec3<double>( std::numeric_limits<double>::quiet_NaN(),
                                                                    std::numeric_limits<double>::quiet_NaN(),
                                                                    std::numeric_limits<double>::quiet_NaN() );

        //long int JawPairsX = 0;                  // (300a,00b8) CS [ASYMX]
        std::vector<double> JawPositionsX;       // (300a,00bc) IS [1] 

        //long int JawPairsY = 0;                  // (300a,00b8) CS [ASYMY]
        std::vector<double> JawPositionsY;       // (300a,00bc) IS [1] 

        //long int MLCPairsX = 0;                  // (300a,00b8) CS [MLCX]
        std::vector<double> MLCPositionsX;       // (300a,00bc) IS [60]

        //long int MLCPairsY = 0;                  // (300a,00b8) CS [MLCY]
        //std::vector<double> MLCPositionsY;       // (300a,00bc) IS [60]
        
        std::map< std::string, std::string > metadata; //User-defined metadata.

        //Constructor/Destructors.
        Static_Machine_State();
        Static_Machine_State(const Static_Machine_State &rhs); //Performs a deep copy (unless copying self).

        //Member functions.
        Static_Machine_State & operator=(const Static_Machine_State &rhs); //Performs a deep copy (unless copying self).

        template <class U> std::optional<U> GetMetadataValueAs(const std::string& key) const;
};

// The class represents a dynamic configuration of a treatment machine composed of interpolated static states.
class Dynamic_Machine_State {
    public:

        long int BeamNumber = -1;

        double FinalCumulativeMetersetWeight = std::numeric_limits<double>::quiet_NaN();


        std::vector<Static_Machine_State> static_states; // Each static state represents a keyframe.

        std::map< std::string, std::string > metadata; //User-defined metadata.

        //Constructor/Destructors.
        Dynamic_Machine_State();
        Dynamic_Machine_State(const Dynamic_Machine_State &rhs); //Performs a deep copy (unless copying self).

        //Member functions.
        Dynamic_Machine_State & operator=(const Dynamic_Machine_State &rhs); //Performs a deep copy (unless copying self).

        void sort_states(); // Sorts static states so that CumulativeMetersetWeight monotonically increases.
        bool verify_states_are_ordered() const; // Ensures the static states are ordered and none are missing.
        void normalize_states(); // Replaces NaNs with previously specified static states, where possible.
        Static_Machine_State interpolate(double CumulativeMetersetWeight) const; // Interpolates adjacent states.

        template <class U> std::optional<U> GetMetadataValueAs(const std::string& key) const;
};

// This class holds a radiotherapy plan, i.e., an arrangement of multiple dynamic particle beams and treatment
// structures (e.g., treatment couch) combined with other relevant information, such as prescription dose.
class TPlan_Config {
    public:

        std::vector<Dynamic_Machine_State> dynamic_states; // Each dynamic state represents a unique beam.

        std::map< std::string, std::string > metadata; //User-defined metadata.

        //Constructor/Destructors.
        TPlan_Config();
        TPlan_Config(const TPlan_Config &rhs); //Performs a deep copy (unless copying self).

        //Member functions.
        TPlan_Config & operator=(const TPlan_Config &rhs); //Performs a deep copy (unless copying self).

        template <class U> std::optional<U> GetMetadataValueAs(const std::string& key) const;
};


// This class is meant to hold a one-dimensional scalar array of data. For example, it could hold a time
// course or dose-volume histogram.
class Line_Sample {
    public:

        samples_1D<double> line;

        //Constructor/Destructors.
        Line_Sample();
        Line_Sample(const Line_Sample &rhs); //Performs a deep copy (unless copying self).

        //Member functions.
        Line_Sample & operator=(const Line_Sample &rhs); //Performs a deep copy (unless copying self).
};


// This class holds a generic vector transformation in $R^3$. It is designed to hold a variety of transformations,
// ranging in complexity from simple Affine transformations to full-blown deformable registrations.
class Transform3 {
    public:

        std::variant< std::monostate,
                      affine_transform<double>,
                      thin_plate_spline > transform;

        std::map< std::string, std::string > metadata; //User-defined metadata.

        //Constructor/Destructors.
        Transform3();
        Transform3(const Transform3 &rhs); //Performs a deep copy (unless copying self).

        //Member functions.
        Transform3 & operator=(const Transform3 &rhs); //Performs a deep copy (unless copying self).

        template <class U> std::optional<U> GetMetadataValueAs(const std::string& key) const;
};


//---------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------ Drover -------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------
//This class collects pixel data from CTs and dose files and mixes them with contours. Formally, this class only tracks pointers to these objects.
// This class exists to help facilitate the mixing of data of various types so that the algorithms are not cluttered with awkward references to 
// all the different objects separately. This class does not partition files into logical groups -- this needs to be done manually or using the
// file_wrangler class.
//
//Etymology: The term "drover" is defined as: 'a person whose occupation is the driving of sheep or cattle, esp to and from market'
// (from http://www.collinsdictionary.com/dictionary/english/drover .) This class is used to hold/wrangle/deal with the data we grab from DICOM files.
//
//The choice of shared_ptr here is so that we can easily copy and swap parts in and out. For instance, some operations on Contour_Data will return 
// additional instances of Contour_Data. If we want to compute something within the Drover using this new Contour_Data we either have to temporarily
// grab the unique_ptrs and swap in/out the new data, perform the computation, and swap them all back (possibly transferring the pointers back out.)
// Shared_ptrs allow us to simply copy Drover instances (at the cost of duplicating a few pointers) and performing computations with convenient 
// member functions (and no dealing with pointer swapping, etc..)
//
//If you are still muttering the various dogmas about 'shared_ptrs are bad because they told me not to use them,' consider that this class is 
// not designed to be passed around. That is, the ownership of instances of this class are intended to remain firmly within the grasp of the creator.
// If anything, they will be 'borrowed,' temporarily, for single-level computations. The underlying image data buffer are actually (at the time of
// writing) unique_ptrs, so the shared_ptrs are a 'butter layer' over the actual data. This class is merely a way to have both "single producers"/
// "single data stewards" and "multiple consumers" with the major benefits of each.
//

//Helper functions - unordered_map with iterator or pointer key types. Sorting/hashing is performed using the address of the object 
// being pointed to. This is extremely convenient, but quite unsafe. Do not use if you do not understand how to avoid segfaults!
using bnded_dose_map_key_t = std::list<contour_collection<double>>::iterator; //Required be an iterator or pointer.

using bnded_dose_map_cmp_func_t = std::function<bool (const bnded_dose_map_key_t &, const bnded_dose_map_key_t &)>;


const auto bnded_dose_map_cmp_lambda = [](const bnded_dose_map_key_t &A, const bnded_dose_map_key_t &B) -> bool {
    //Given that we are dealing with iterators, we must be careful with possibly-invalid data sent to 
    // this function!

    //It is desirable to order the same on every machine so that output data can be more easily compared.
 
    //First, we check the structure name(s).
    {
        const auto A_names = A->get_distinct_values_for_key("ROIName");
        const auto B_names = B->get_distinct_values_for_key("ROIName");
        if(A_names != B_names) return A_names < B_names;
    }
    
    //Next we check the segmentation history.
    {
        const auto A_seghist = A->get_distinct_values_for_key("SegmentationHistory");
        const auto B_seghist = B->get_distinct_values_for_key("SegmentationHistory");
        if(A_seghist != B_seghist) return A_seghist < B_seghist;
    }

    //Next check the number of contours.

    // ... 

    //Next, use the built-in operator<.

    // ... 

    //Next check the address for equality. This is machine portable and consistent.
    if(reinterpret_cast<size_t>(&(*A)) == reinterpret_cast<size_t>(&(*B))) return false;

    //Finally, we resort to operator<'ing the casted address. Note that this *will* 
    // vary from machine to machine, but it may or may not ever be a problem. 
    // If we get here, issue a warning.
    // 
    FUNCWARN("Resorting to a less-than on addresses. This will produce possibly non-reproduceable sorting order! Fix me if required");
    return reinterpret_cast<size_t>(&(*A)) < reinterpret_cast<size_t>(&(*B));
};

typedef std::map<bnded_dose_map_key_t,double,                                   bnded_dose_map_cmp_func_t>  drover_bnded_dose_mean_dose_map_t;
typedef std::map<bnded_dose_map_key_t,vec3<double>,                             bnded_dose_map_cmp_func_t>  drover_bnded_dose_centroid_map_t;
typedef std::map<bnded_dose_map_key_t,std::list<double>,                        bnded_dose_map_cmp_func_t>  drover_bnded_dose_bulk_doses_map_t;
typedef std::map<bnded_dose_map_key_t,std::pair<int64_t,int64_t>,               bnded_dose_map_cmp_func_t>  drover_bnded_dose_accm_dose_map_t;
typedef std::map<bnded_dose_map_key_t,std::pair<double,double>,                 bnded_dose_map_cmp_func_t>  drover_bnded_dose_min_max_dose_map_t;
typedef std::map<bnded_dose_map_key_t,std::tuple<double,double,double>,         bnded_dose_map_cmp_func_t>  drover_bnded_dose_min_mean_max_dose_map_t;
typedef std::map<bnded_dose_map_key_t,std::tuple<double,double,double,double>,  bnded_dose_map_cmp_func_t>  drover_bnded_dose_min_mean_median_max_dose_map_t;

typedef std::tuple<vec3<double>,vec3<double>,vec3<double>,double,long int,long int> bnded_dose_pos_dose_tup_t;
typedef std::map<bnded_dose_map_key_t,std::list<bnded_dose_pos_dose_tup_t>,         bnded_dose_map_cmp_func_t>  drover_bnded_dose_pos_dose_map_t; 
typedef std::map<bnded_dose_map_key_t,std::map<std::array<int,3>,double>,           bnded_dose_map_cmp_func_t>  drover_bnded_dose_stat_moments_map_t;

drover_bnded_dose_mean_dose_map_t                drover_bnded_dose_mean_dose_map_factory();
drover_bnded_dose_centroid_map_t                 drover_bnded_dose_centroid_map_factory();
drover_bnded_dose_bulk_doses_map_t               drover_bnded_dose_bulk_doses_map_factory();
drover_bnded_dose_accm_dose_map_t                drover_bnded_dose_accm_dose_map_factory();
drover_bnded_dose_min_max_dose_map_t             drover_bnded_dose_min_max_dose_map_factory();
drover_bnded_dose_min_mean_max_dose_map_t        drover_bnded_dose_min_mean_max_dose_map_factory();
drover_bnded_dose_min_mean_median_max_dose_map_t drover_bnded_dose_min_mean_median_max_dose_map_factory();
drover_bnded_dose_pos_dose_map_t                 drover_bnded_dose_pos_dose_map_factory();
drover_bnded_dose_stat_moments_map_t             drover_bnded_dose_stat_moments_map_factory();

class Drover {
    public:

        std::shared_ptr<Contour_Data>            contour_data; //Should I allow for multiple contour data? I think it would be logical...
        std::list<std::shared_ptr<Image_Array>>  image_data;   //In case we ever get more than one set of images (different modalities?)
        std::list<std::shared_ptr<Point_Cloud>>  point_data;
        std::list<std::shared_ptr<Surface_Mesh>> smesh_data;
        std::list<std::shared_ptr<TPlan_Config>> tplan_data;
        std::list<std::shared_ptr<Line_Sample>>  lsamp_data;
        std::list<std::shared_ptr<Transform3>>   trans_data;
    
        //Constructors.
        Drover();
        Drover(const Drover &in);
    
        //Member functions.
        void operator = (const Drover &rhs);
        void Bounded_Dose_General( std::list<double> *pixel_doses, 
                                   drover_bnded_dose_bulk_doses_map_t *bulk_doses, //NOTE: Similar to pixel_doses, but not all in a single bunch.
                                   drover_bnded_dose_mean_dose_map_t *mean_doses, 
                                   drover_bnded_dose_min_max_dose_map_t *min_max_doses,
                                   drover_bnded_dose_pos_dose_map_t *pos_doses,
                                   const std::function<bool(bnded_dose_pos_dose_tup_t)>& Fselection,
                                   drover_bnded_dose_stat_moments_map_t *centralized_moments ) const;
    
        std::list<double> Bounded_Dose_Bulk_Values() const;                 //If the contours contain multiple organs, we get TOTAL bulk pixel values (Gy or cGy?)
        drover_bnded_dose_mean_dose_map_t Bounded_Dose_Means() const;       //Get mean dose for each contour collection. See note in source.
        drover_bnded_dose_min_max_dose_map_t Bounded_Dose_Min_Max() const;  //Get the min & max dose for each contour collection. See note in source.
        drover_bnded_dose_min_mean_max_dose_map_t Bounded_Dose_Min_Mean_Max() const;  // " " " " ...
        drover_bnded_dose_min_mean_median_max_dose_map_t Bounded_Dose_Min_Mean_Median_Max() const; // " " " " ...
        drover_bnded_dose_stat_moments_map_t Bounded_Dose_Centralized_Moments() const;
        drover_bnded_dose_stat_moments_map_t Bounded_Dose_Normalized_Cent_Moments() const;
    
        Drover Segment_Contours_Heuristically(const std::function<bool(bnded_dose_pos_dose_tup_t)>& heur) const;
    
        std::pair<double,double> Bounded_Dose_Limits() const;  //Returns the min and max voxel doses (in cGy or Gy?) amongst ALL contour-enclosed voxels.
        std::map<double,double>  Get_DVH() const;              //If the contours contain multiple organs, we get TOTAL (cumulative) DVH (in Gy or cGy?)
    
        Drover Duplicate(std::shared_ptr<Contour_Data> in) const; //Duplicates all but our the contours. Inserts those passed in instead.
        Drover Duplicate(const Contour_Data &in) const; 
        Drover Duplicate(const Drover &in) const;
    
        bool Has_Contour_Data() const;
        bool Has_Image_Data() const;
        bool Has_Point_Data() const;
        bool Has_Mesh_Data() const;
        bool Has_TPlan_Data() const;
        bool Has_LSamp_Data() const;
        bool Has_Tran3_Data() const;

        void Ensure_Contour_Data_Allocated();

        void Concatenate(const std::shared_ptr<Contour_Data>& in);
        void Concatenate(std::list<std::shared_ptr<Image_Array>> in);
        void Concatenate(std::list<std::shared_ptr<Point_Cloud>> in);
        void Concatenate(std::list<std::shared_ptr<Surface_Mesh>> in);
        void Concatenate(std::list<std::shared_ptr<TPlan_Config>> in);
        void Concatenate(std::list<std::shared_ptr<Line_Sample>> in);
        void Concatenate(std::list<std::shared_ptr<Transform3>> in);
        void Concatenate(Drover in);

        void Consume(const std::shared_ptr<Contour_Data>& in);
        void Consume(std::list<std::shared_ptr<Image_Array>> in);
        void Consume(std::list<std::shared_ptr<Point_Cloud>> in);
        void Consume(std::list<std::shared_ptr<Surface_Mesh>> in);
        void Consume(std::list<std::shared_ptr<TPlan_Config>> in);
        void Consume(std::list<std::shared_ptr<Line_Sample>> in);
        void Consume(std::list<std::shared_ptr<Transform3>> in);
        void Consume(Drover in);
    
        void Plot_Dose_And_Contours() const;
        void Plot_Image_Outlines() const;
};



using icase_str_lt_func_t = std::function<bool (const std::string &, const std::string)>;
template< class T >
using icase_map_t = std::map<std::string, T, icase_str_lt_func_t>;
auto icase_str_lt_lambda = [](const std::string &A, const std::string &B) -> bool {
    std::string AA(A);
    std::string BB(B);
    std::transform(AA.begin(), AA.end(), AA.begin(), [](unsigned char c){ return std::tolower(c); });
    std::transform(BB.begin(), BB.end(), BB.begin(), [](unsigned char c){ return std::tolower(c); });
    return std::lexicographical_compare(std::begin(AA), std::end(AA), std::begin(BB), std::end(BB));
//    return std::lexicographical_compare(std::begin(A), std::end(A), std::begin(B), std::end(B));
};
auto icase_str_eq_lambda = [](const std::string &A, const std::string &B) -> bool {
    std::string AA(A);
    std::string BB(B);
    std::transform(AA.begin(), AA.end(), AA.begin(), [](unsigned char c){ return std::tolower(c); });
    std::transform(BB.begin(), BB.end(), BB.begin(), [](unsigned char c){ return std::tolower(c); });
    return (AA == BB);
//    return (A == B);
};


// Class for dealing with commandline argument operation options.
//
// Argument case is retained, but case insensitivity is used when pushing back new arguments.
// So you get out what you put it, but the class won't accept both 'foo' and 'FOO'.
//
class OperationArgPkg {

    private:
        std::string name; // The operation name.
        icase_map_t<std::string> opts; // Arguments to pass to the operation.

        std::list<OperationArgPkg> children; // Child nodes that can be interpretted in different ways.

    public:
        OperationArgPkg(std::string unparsed, const std::string& sepr = ":", const std::string& eqls = "="); // e.g., "SomeOperation:keyA=valueA:keyB=valueB"
        OperationArgPkg(const OperationArgPkg &) = default;

        OperationArgPkg & operator=(const OperationArgPkg &rhs);

        //Accessor.
        std::string getName() const;

        //Checks if the provided keys (and only the provided keys) are present.
        bool containsExactly(std::initializer_list<std::string> l) const;

        std::optional<std::string> getValueStr(const std::string& key) const;

        bool insert(const std::string& key, std::string val); //Will not overwrite.
        bool insert(const std::string& keyval); //Will not overwrite.

        //Children.
        void makeChild(std::string unparsed, const std::string& sepr = ":", const std::string& eqls = "=");
        void makeChild(const OperationArgPkg &);

        std::list<OperationArgPkg> getChildren() const;
        OperationArgPkg* lastChild(); //Non-owning pointer, mostly useful for adding arguments during command line parsing.
};


enum class OpArgVisibility {
    // This class is used to conceal operation arguments from user-facing interfaces.
    //
    // Note: This setting will not affect visibility for the operation itself.
    Show,
    Hide,
};

enum class OpArgFlow {
    // This class is used to denote whether operation arguments are used as inputs (ingress) or outputs (egress).
    Ingress,
    Egress,
    IngressEgress,
    Unknown,
};

enum class OpArgSamples {
    // This class is used to denote whether the provided samples are examples or are an exhaustive list of all options.
    Examples,
    Exhaustive,
};

// Class for documenting commandline argument operation options.
struct OperationArgDoc {
    std::string name;
    std::string desc;
    std::string default_val;
    bool expected;
    std::list<std::string> examples;

    // Auxiliary information.
    std::string mimetype; // If applicable.

    OpArgVisibility visibility = OpArgVisibility::Show;
    OpArgFlow flow = OpArgFlow::Unknown;
    OpArgSamples samples = OpArgSamples::Examples;

};


// Class for wrapping documentation about an operation, including the operation itself and descriptions of parameters.
struct OperationDoc {
    std::list<OperationArgDoc> args; // Documentation for the arguments. 

    std::string name; // Documentation for the operation itself.
    std::list<std::string> aliases; // Other names for the operation itself.
    std::string desc; // Documentation for the operation itself.
    std::list<std::string> notes; // Special notes concerning the operation, usually caveats or notices.

};

