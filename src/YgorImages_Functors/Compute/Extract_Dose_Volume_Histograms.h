//Extract_Dose_Volume_Histograms.h.
#pragma once

#include <cmath>
#include <any>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "YgorImages.h"
#include "YgorMath.h"
#include "YgorMisc.h"

template <class T, class R> class planar_image_collection;
template <class T> class contour_collection;

struct ComputeExtractDoseVolumeHistogramsUserData {

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
    // The channel to consider. 
    //
    // Note: Channel numbers in the images that will be edited and reference images must match.
    //       Negative values will use all channels.
    //
    long int channel = -1;

    // -----------------------------
    // Outgoing cumulative dose-volume histograms, one for each distinct ROI.
    //
    std::map<std::string,                         // ROIName.
             std::map<double,                     // Dose (in DICOM units; mm).
                      std::pair<double,           // Cumulative volume (in DICOM units; mm^3).
                                double> > > dvhs; // Cumulative volume (relative to the ROI's total volume [0,1]).

    // -----------------------------
    // Outgoing basic dose statistics.
    //
    std::map<std::string,                         // ROIName.
             double> min_dose;                    // Minimum dose (in DICOM units; mm).

    std::map<std::string,                         // ROIName.
             double> max_dose;                    // Maximum dose (in DICOM units; mm).

    std::map<std::string,                         // ROIName.
             double> mean_dose;                   // Mean volume dose (weighted by voxel volume, in DICOM units; mm).

};

bool ComputeExtractDoseVolumeHistograms(planar_image_collection<float,double> &,
                          std::list<std::reference_wrapper<planar_image_collection<float,double>>>,
                          std::list<std::reference_wrapper<contour_collection<double>>>,
                          std::any ud );

