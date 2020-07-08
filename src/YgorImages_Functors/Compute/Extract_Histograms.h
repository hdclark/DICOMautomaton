//Extract_Histograms.h.
#pragma once

#include <any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <string>

#include "YgorImages.h"
#include "YgorMath.h"

template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;

struct ComputeExtractHistogramsUserData {

    // -----------------------------
    // Settings that control how contours are interpretted.
    //
    // Note: Some settings will are set internally, so user settings may be overridden.
    //
    Mutate_Voxels_Opts mutation_opts;
    //mutation_opts.editstyle      = Mutate_Voxels_Opts::EditStyle::InPlace;
    //mutation_opts.inclusivity    = Mutate_Voxels_Opts::Inclusivity::Centre;
    //mutation_opts.contouroverlap = Mutate_Voxels_Opts::ContourOverlap::Ignore;
    //mutation_opts.aggregate      = Mutate_Voxels_Opts::Aggregate::First;
    //mutation_opts.adjacency      = Mutate_Voxels_Opts::Adjacency::SingleVoxel;
    //mutation_opts.maskmod        = Mutate_Voxels_Opts::MaskMod::Noop;

    // -----------------------------
    // The width of histogram bins, in DICOM units (nominally Gy).
    //
    double dDose = 1.0;

    // -----------------------------
    // The (inclusive) range of voxels to consider, in DICOM units (nominally Gy).
    //
    double lower_threshold = -(std::numeric_limits<double>::infinity());
    double upper_threshold =  (std::numeric_limits<double>::infinity());

    // -----------------------------
    // The channel to consider. 
    //
    // Note: Channel numbers in the images that will be edited and reference images must match.
    //       Negative values will use all channels.
    //
    long int channel = -1;

    // -----------------------------
    // How contours with differing names should be handled.
    enum class
    GroupingMethod {
        Separate,     // Contours with the same ROI label will be treated as part of the same ROI.
        Combined,     // Contours with different ROI labels will all be treated as a single ROI.
                      // The label attached to the output is not defined.
    } grouping = GroupingMethod::Separate;

    // -----------------------------
    // Outgoing histograms, one for each distinct group.
    //
    std::map<std::string,            // ROIName.
             samples_1D<double>>     // Volume vs voxel intensity in DICOM units; mm^3 and Gy.
                 differential_histograms;
    std::map<std::string,            // ROIName.
             samples_1D<double>>     // Volume vs voxel intensity in DICOM units; mm^3 and Gy.
                 cumulative_histograms;

};

bool ComputeExtractHistograms(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

