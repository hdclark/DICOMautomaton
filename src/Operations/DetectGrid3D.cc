//DetectGrid3D.cc - A part of DICOMautomaton 2019. Written by hal clark.

#include <experimental/any>
#include <experimental/optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/ConvenienceRoutines.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Compute/Volumetric_Neighbourhood_Sampler.h"
#include "DetectGrid3D.h"
#include "YgorImages.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"       //Needed for Stats:: namespace.

/*
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>

#include <boost/geometry/index/parameters.hpp>
#include <boost/geometry/index/rtree.hpp>

#include <boost/geometry/algorithms/within.hpp>
*/

#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/Eigenvalues>

#include "YgorClustering.hpp"


OperationDoc OpArgDocDetectGrid3D(void){
    OperationDoc out;
    out.name = "DetectGrid3D";

    out.desc = 
        "This routine fits a 3D grid to a point cloud using a Procrustes analysis with "
        " point-to-model correspondence estimated via an iterative closest point approach.";
    
    out.notes.emplace_back(
        "Traditional Procrustes analysis requires a priori point-to-point correspondence knowledge."
        " Because this operation fits a model (with infinite extent), point-to-point correspondence"
        " is not known and the model is effectively an infinite continuum of potential points."
        " To overcome this problem, correspondence is estimated by projecting each point in the point"
        " cloud onto every grid line and selecting the closest projected point."
        " The point cloud point and the project point are then treated as corresponding points."
        " Using this phony correspondence, the Procrustes problem is solved and the grid is reoriented."
        " This is performed iteratively. However **there is no guarantee the procedure will converge**"
        " and furthermore, even if it does converge, **there is no guarantee that the grid will be"
        " appropriately fit**. The best results will occur when the grid is already closely aligned"
        " with the point cloud (i.e., when the first guess is very close to a solution)."
        " If this cannot be guaranteed, it may be advantageous to have a nearly continuous point"
        " cloud to avoid gaps in which the iteration can get stuck in a local minimum."
        " If this cannot be guaranteed, it might be worthwhile to perform a bootstrap (or RANSAC)"
        " and discard all but the best fit."
    );

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelection";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "GridSeparation";
    out.args.back().desc = "The separation of the grid (in DICOM units; mm) being fit."
                           " This parameter describes how close adjacent grid lines are to one another."
                           " Separation is measured from one grid line centre to the nearest adjacent grid line centre.";
    out.args.back().default_val = "10.0";
    out.args.back().expected = true;
    out.args.back().examples = { "10.0", 
                                 "15.5",
                                 "25.0",
                                 "1.23E4" };

    out.args.emplace_back();
    out.args.back().name = "LineThickness";
    out.args.back().desc = "The thickness of grid lines (in DICOM units; mm)."
                           " If zero, lines are treated simply as lines."
                           " If non-zero, grid lines are treated as hollow cylinders with a diameter of this thickness.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1.0", 
                                 "1.5",
                                 "10.0",
                                 "1.23E4" };

    return out;
}

Drover DetectGrid3D(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionStr = OptArgs.getValueStr("PointSelection").value();

    const auto GridSeparation = std::stod( OptArgs.getValueStr("GridSeparation").value() );
    const auto LineThickness = std::stod( OptArgs.getValueStr("LineThickness").value() );

    //-----------------------------------------------------------------------------------------------------------------

    if(!std::isfinite(GridSeparation) || (GridSeparation <= 0.0)){
        throw std::invalid_argument("Grid separation is not valid. Cannot continue.");
    }
    if(!std::isfinite(LineThickness) || (LineThickness <= 0.0)){
        throw std::invalid_argument("Line thickness is not valid. Cannot continue.");
    }
    if(GridSeparation < LineThickness){
        throw std::invalid_argument("Line thickness is impossible with given grid spacing. Refusing to continue.");
    }

    auto PCs_all = All_PCs( DICOM_data );
    auto PCs = Whitelist( PCs_all, PointSelectionStr );
    for(auto & pcp_it : PCs){

        const vec3<double> x_unit(1.0, 0.0, 0.0);
        const vec3<double> y_unit(0.0, 1.0, 0.0);
        const vec3<double> z_unit(0.0, 0.0, 1.0);

        vec3<double> current_grid_x = x_unit;
        vec3<double> current_grid_y = y_unit;
        vec3<double> current_grid_z = z_unit;
        vec3<double> current_grid_anchor(0.0, 0.0, 0.0);

        Stats::Running_MinMax<float> rmm_x;
        Stats::Running_MinMax<float> rmm_y;
        Stats::Running_MinMax<float> rmm_z;

        // Determine point cloud bounds.
        for(const auto &pp : (*pcp_it)->points){
            rmm_x.Digest( pp.first.Dot(x_unit) );
            rmm_y.Digest( pp.first.Dot(y_unit) );
            rmm_z.Digest( pp.first.Dot(z_unit) );
        }

        // Determine where to centre the grid.
        //
        // TODO: Use the median here instead of mean. It will more often correspond to an actual grid line if the
        //       point cloud orientation is comparable to the grid.
        const double x_mid = (rmm_x.Current_Max() + rmm_x.Current_Min()) * 0.5;
        const double y_mid = (rmm_y.Current_Max() + rmm_y.Current_Min()) * 0.5;
        const double z_mid = (rmm_z.Current_Max() + rmm_z.Current_Min()) * 0.5;

        const double x_diff = (rmm_x.Current_Max() - rmm_x.Current_Min());
        const double y_diff = (rmm_y.Current_Max() - rmm_y.Current_Min());
        const double z_diff = (rmm_z.Current_Max() - rmm_z.Current_Min());

        FUNCINFO("x width = " << x_mid << " +- " << x_diff * 0.5);
        FUNCINFO("y width = " << y_mid << " +- " << y_diff * 0.5);
        FUNCINFO("z width = " << z_mid << " +- " << z_diff * 0.5);

        current_grid_anchor = vec3<double>(x_mid, y_mid, z_mid);

        // Determine initial positions for planes.
        //
        // They should be spaced evenly and should adaquately cover the point cloud with at least one grid line margin.
        std::list<plane<double>> planes;

        for(size_t i = 0; ; ++i){
            const auto dR = static_cast<double>(i) * GridSeparation;

            //plane(const vec3<T> &N_0_in, const vec3<T> &R_0_in);
            const auto N_0 = current_grid_x;

            planes.emplace_back(N_0,
                                current_grid_anchor + current_grid_x * dR);
            if(i != 0){
                planes.emplace_back(N_0,
                                    current_grid_anchor - current_grid_x * dR);
            }

            if(dR >= (x_diff * 0.5 + GridSeparation)) break;
        }










         

        // Creates planes for all grid intersections.
            

FUNCERR("This routine has not yet been implemented. Refusing to continue");

    }

    return DICOM_data;
}
