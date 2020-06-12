//ExtractPointsWarp.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <asio.hpp>
#include <algorithm>
#include <optional>
#include <fstream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set> 
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Thread_Pool.h"

#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "ExtractPointsWarp.h"

OperationDoc OpArgDocExtractPointsWarp(void){
    OperationDoc out;
    out.name = "ExtractPointsWarp";

    out.desc = 
        "This operation uses two point clouds (one 'moving' and the other 'stationary' or 'reference') to find a"
        " transformation ('warp') that will map the moving point set to the stationary point set."
        " The resulting transformation encapsulates a 'registration' between the two point sets -- however the"
        " transformation is generic and can be later be used to move (i.e., 'warp', 'deform') other objects,"
        " including the 'moving' point set.";
        
    out.notes.emplace_back(
        "The 'moving' point cloud is *not* warped by this operation -- this operation merely identifies a suitable"
        " transformation. Separation of the identification and application of a warp allows the warp to more easily"
        " re-used and applied to multiple objects."
    );
    out.notes.emplace_back(
        "The output of this operation is a transformation that can later be applied, in principle, to point clouds,"
        " surface meshes, images, arbitrary vector fields, and any other objects in $R^{3}$."
    );
    out.notes.emplace_back(
        "There are multiple algorithms implemented. Some do *not* provide bijective mappings, meaning that swapping"
        " the inputs will result in an altogether different registration (even after inverting it)."
    );
#ifdef DCMA_USE_EIGEN
#else
    out.notes.emplace_back(
        "Functionality provided by Eigen has been disabled. The available transformation methods have been reduced."
    );
#endif

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "MovingPointSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The point cloud that will serve as input to the warp function. "_s
                         + out.args.back().desc;


    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "ReferencePointSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The stationary point cloud to use as a reference for the moving point cloud. "_s
                         + out.args.back().desc
                         + " Note that this point cloud is not modified.";


    out.args.emplace_back();
    out.args.back().name = "Method";
    out.args.back().desc = "The alignment algorithm to use."
                           " The following alignment options are available: 'centroid'"
#ifdef DCMA_USE_EIGEN
                           ", 'PCA', 'exhaustive_icp', 'TPS', and 'TPS-RPM'"
#endif
                           "."
                           " The 'centroid' option finds a rotationless translation the aligns the centroid"
                           " (i.e., the centre of mass if every point has the same 'mass')"
                           " of the moving point cloud with that of the stationary point cloud."
                           " It is susceptible to noise and outliers, and can only be reliably used when the point"
                           " cloud has complete rotational symmetry (i.e., a sphere). On the other hand, 'centroid'"
                           " alignment should never fail, can handle a large number of points,"
                           " and can be used in cases of 2D and 1D degeneracy."
                           " centroid alignment is frequently used as a pre-processing step for more advanced algorithms."
                           ""
#ifdef DCMA_USE_EIGEN
                           " The 'PCA' option finds an Affine transformation by performing centroid alignment,"
                           " performing principle component analysis (PCA) separately on the reference and moving"
                           " point clouds, computing third-order point distribution moments along each principle axis"
                           " to establish a consistent orientation,"
                           " and then rotates the moving point cloud so the principle axes of the stationary and"
                           " moving point clouds coincide."
                           " The 'PCA' method may be suitable when: (1) both clouds are not contaminated with extra"
                           " noise points (but some Gaussian noise in the form of point 'jitter' should be tolerated)"
                           " and (2) the clouds are not perfectly spherical (i.e., so they have valid principle"
                           " components)."
                           " However, note that the 'PCA' method is susceptible to outliers and can not scale"
                           " a point cloud."
                           " The 'PCA' method will generally fail when the distribution of points shifts across the"
                           " centroid (i.e., comparing reference and moving point clouds) since the orientation of"
                           " the components will be inverted, however 2D degeneracy is handled in a 3D-consistent way,"
                           " and 1D degeneracy is handled in a 1D-consistent way (i.e, the components orthogonal to"
                           " the common line will be completely ambiguous, so spurious rotations will result)."
                           ""
                           " The 'exhaustive_icp' option finds an Affine transformation by first performing PCA-based"
                           " alignment and then iteratively alternating between (1) estimating point-point"
                           " correspondence and (1) solving for a least-squares optimal transformation given this"
                           " correspondence estimate. 'ICP' stands for 'iterative closest point.'"
                           " Each iteration uses the previous transformation *only* to estimate correspondence;"
                           " a least-squares optimal linear transform is estimated afresh each iteration."
                           " The 'exhaustive_icp' method is most suitable when both point clouds consist of"
                           " approximately 50k points or less. Beyond this, ICP will still work but runtime"
                           " scales badly."
                           " ICP is susceptible to outliers and will not scale a point cloud."
                           " It can be used for 2D and 1D degenerate problems, but is not guaranteed to find the"
                           " 'correct' orientation of degenerate or symmetrical point clouds."
                           ""
                           " The 'TPS' or Thin-Plate Spline algorithm provides non-rigid"
                           " (i.e., 'deformable') registration between corresponding point sets."
                           " The moving and stationary point sets must have the same number of points, and"
                           " the $n$^th^ moving point is taken to correspond to the $n$^th^ stationary point."
                           " The 'TPS' method does not scale well due in part to inversion of a large (NxN) matrix"
                           " and is therefore most suitable when both point clouds"
                           " consist of approximately 10-20k points or less. Beyond this, expect slow calculations."
                           " The TPS method is not robust to outliers, however a regularization parameter can be used"
                           " to control the smoothness of the warp. (Setting to zero will cause the warp function to"
                           " exactly interpolate every pair, except due to floating point inaccuracies.)"
                           " Also note that the TPS method can only, in general, be used for interpolation."
                           " Extrapolation beyond the points clouds will"
                           " almost certainly result in wildly inconsistent and unstable transformations."
                           " Consult Bookstein 1989 (doi:10.1109/34.24792) for an overview."
                           ""
                           " The 'TPS-RPM' or Thin-Plate Spline Robust Point-Matching algorithm provides non-rigid"
                           " (i.e., 'deformable') registration."
                           " It combines a soft-assign technique, deterministic annealing, and"
                           " thin-plate splines to iteratively solve for correspondence and spatial warp."
                           " The 'TPS-RPM' method is (somewhat) robust to outliers in both moving and stationary point"
                           " sets, but it suffers from numerical instabilities when one or more inputs are degenerate"
                           " or symmetric in such a way that many potential solutions have the same least-square cost."
                           " The 'TPS-RPM' method does not scale well due in part to inversion of a large (NxM) matrix"
                           " and is therefore most suitable when both point clouds"
                           " consist of approximately 1-5k points or less. Beyond this, expect slow calculations."
                           " Also note that the underlying TPS method can only, in general, be used for interpolation."
                           " Extrapolation beyond the extent of the corresponding parts of the points clouds will"
                           " almost certainly result in wildly inconsistent and unstable transformations."
                           " Consult Chui and Rangarajan 2000 (original algorithm; doi:10.1109/CVPR.2000.854733)"
                           " and Yang 2011 (clarification and more robust solution; doi:10.1016/j.patrec.2011.01.015)"
                           " for more details."
                           ""
#endif
                           "";
    out.args.back().default_val = "centroid";
    out.args.back().expected = true;
#ifdef DCMA_USE_EIGEN
    out.args.back().examples = { "centroid", "pca", "exhaustive_icp", "tps", "tps_rpm" };
#else
    out.args.back().examples = { "centroid" };
#endif

#ifdef DCMA_USE_EIGEN
    out.args.emplace_back();
    out.args.back().name = "TPSLambda";
    out.args.back().desc = "Regularization parameter for the TPS method."
                           " Controls the smoothness of the fitted thin plate spline function."
                           " Setting to zero will ensure that all points are interpolated exactly (barring numerical"
                           " imprecision). Setting higher will allow the spline to 'relax' and smooth out."
                           " The specific value to use is heavily dependent on the problem domain and the amount"
                           " of noise and outliers in the data. It relates to the spacing between points."
                           " Note that this parameter is used with the TPS method, but *not* in the TPS-RPM method.";
    out.args.back().default_val = "0.0";
    out.args.back().expected = true;
    out.args.back().examples = { "1E-4",
                                 "0.1",
                                 "10.0", };
#endif

#ifdef DCMA_USE_EIGEN
    out.args.emplace_back();
    out.args.back().name = "TPSKernelDimension";
    out.args.back().desc = "Dimensionality of the spline function kernel."
                           " The kernel dimensionality *should* match the dimensionality of the points (i.e., 3),"
                           " but doesn't need to."
                           " 2 seems to work best, even with points in 3D."
                           " Note that this parameter may affect how the transformation extrapolates.";
    out.args.back().default_val = "2";
    out.args.back().expected = true;
    out.args.back().examples = { "2",
                                 "3", };
#endif

#ifdef DCMA_USE_EIGEN
    out.args.emplace_back();
    out.args.back().name = "TPSSolver";
    out.args.back().desc = "The method used to solve the system of linear equtions that defines the thin plate spline"
                           " solution. The pseudoinverse will likely be able to provide a solution when the system is"
                           " degenerate, but it might not be reasonable or even sensible. The LDLT method scales"
                           " better.";
    out.args.back().default_val = "LDLT";
    out.args.back().expected = true;
    out.args.back().examples = { "LDLT",
                                 "PseudoInverse", };
#endif

    out.args.emplace_back();
    out.args.back().name = "MaxIterations";
    out.args.back().desc = "If the method is iterative, only permit this many iterations to occur."
                           " Note that this parameter will not have any effect on non-iterative methods.";
    out.args.back().default_val = "100";
    out.args.back().expected = true;
    out.args.back().examples = { "5",
                                 "20",
                                 "100",
                                 "1000" };


    out.args.emplace_back();
    out.args.back().name = "RelativeTolerance";
    out.args.back().desc = "If the method is iterative, terminate the loop when the cost function changes between"
                           " successive iterations by this amount or less."
                           " The magnitude of the cost function will generally depend on the number of points"
                           " (in both point clouds), the scale (i.e., 'width') of the point clouds, the amount"
                           " of noise and outlier points, and any method-specific"
                           " parameters that impact the cost function (if applicable);"
                           " use of this tolerance parameter may be impacted by these characteristics."
                           " Verifying that a given tolerance is of appropriate magnitude is recommended."
                           " Relative tolerance checks can be disabled by setting to non-finite or negative value."
                           " Note that this parameter will only have effect on iterative methods that are not"
                           " controlled by, e.g., an annealing schedule.";
    out.args.back().default_val = "nan";
    out.args.back().expected = true;
    out.args.back().examples = { "-1",
                                 "1E-2",
                                 "1E-3",
                                 "1E-5" };

    return out;
}



Drover ExtractPointsWarp(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MovingPointSelectionStr = OptArgs.getValueStr("MovingPointSelection").value();
    const auto ReferencePointSelectionStr = OptArgs.getValueStr("ReferencePointSelection").value();

    const auto MethodStr = OptArgs.getValueStr("Method").value();

#ifdef DCMA_USE_EIGEN
    const auto TPSLambda = std::stod( OptArgs.getValueStr("TPSLambda").value() );
    const auto TPSKDim = std::stol( OptArgs.getValueStr("TPSKernelDimension").value() );
    const auto TPSSolverStr = OptArgs.getValueStr("TPSSolver").value();
#endif // DCMA_USE_EIGEN

    const auto MaxIters = std::stol( OptArgs.getValueStr("MaxIterations").value() );
    const auto RelativeTol = std::stod( OptArgs.getValueStr("RelativeTolerance").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_com    = Compile_Regex("^ce?n?t?r?o?i?d?$");
#ifdef DCMA_USE_EIGEN    
    const auto regex_pca    = Compile_Regex("^pc?a?$");
    const auto regex_exhicp = Compile_Regex("^ex?h?a?u?s?t?i?v?e?[-_]?i?c?p?$");
    const auto regex_tps    = Compile_Regex("^tp?s?$");
    const auto regex_tpsrpm = Compile_Regex("^tp?s?[-_]?rp?m?$");

    const auto regex_ldlt = Compile_Regex("^LD?L?T?$");
    const auto regex_pinv = Compile_Regex("^ps?e?u?d?o?[-_]?i?n?v?e?r?s?e?$");
#endif // DCMA_USE_EIGEN

    auto PCs_all = All_PCs( DICOM_data );
    auto ref_PCs = Whitelist( PCs_all, ReferencePointSelectionStr );
    if(ref_PCs.size() != 1){
        throw std::invalid_argument("A single reference point cloud must be selected. Cannot continue.");
    }

    // Iterate over the moving point clouds, aligning each to the reference point cloud.
    auto moving_PCs = Whitelist( PCs_all, MovingPointSelectionStr );
    for(auto & pcp_it : moving_PCs){
        FUNCINFO("There are " << (*ref_PCs.front())->pset.points.size() << " points in the reference point cloud");
        FUNCINFO("There are " << (*pcp_it)->pset.points.size() << " points in the moving point cloud");

        if(false){
        }else if( std::regex_match(MethodStr, regex_com) ){
            auto t_opt = AlignViaCentroid( (*pcp_it)->pset,
                                           (*ref_PCs.front())->pset );
            if(t_opt){
                FUNCINFO("Successfully found warp using centre-of-mass alignment");
                DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( ) );
                DICOM_data.trans_data.back()->transform = t_opt.value();
                DICOM_data.trans_data.back()->metadata["Name"] = "unspecified";
                DICOM_data.trans_data.back()->metadata["WarpType"] = "COM";
            }else{
                throw std::runtime_error("Failed to warp using centre-of-mass alignment.");
            }

#ifdef DCMA_USE_EIGEN    
        }else if( std::regex_match(MethodStr, regex_pca) ){
            auto t_opt = AlignViaPCA( (*pcp_it)->pset,
                                      (*ref_PCs.front())->pset );
            if(t_opt){
                FUNCINFO("Successfully found warp using principle component alignment");
                DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( ) );
                DICOM_data.trans_data.back()->transform = t_opt.value();
                DICOM_data.trans_data.back()->metadata["Name"] = "unspecified";
                DICOM_data.trans_data.back()->metadata["WarpType"] = "PCA";
            }else{
                throw std::runtime_error("Failed to warp using principle component alignment.");
            }
 

        }else if( std::regex_match(MethodStr, regex_exhicp) ){
            auto t_opt = AlignViaExhaustiveICP( (*pcp_it)->pset,
                                                (*ref_PCs.front())->pset,
                                                MaxIters,
                                                RelativeTol );
            if(t_opt){
                FUNCINFO("Successfully found warp using exhaustive ICP");
                DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( ) );
                DICOM_data.trans_data.back()->transform = t_opt.value();
                DICOM_data.trans_data.back()->metadata["Name"] = "unspecified";
                DICOM_data.trans_data.back()->metadata["WarpType"] = "ExhaustiveICP";
            }else{
                throw std::runtime_error("Failed to warp using exhaustive ICP.");
            }

        }else if( std::regex_match(MethodStr, regex_tps) ){
            AlignViaTPSParams params;
            params.lambda = TPSLambda;
            params.kernel_dimension = TPSKDim;
            FUNCINFO("Performing TPS alignment using lambda = " << TPSLambda << " and kdim = " << TPSKDim);

            if(false){
            }else if( std::regex_match(TPSSolverStr, regex_ldlt) ){
                params.solution_method = AlignViaTPSParams::SolutionMethod::LDLT;
            }else if( std::regex_match(TPSSolverStr, regex_pinv) ){
                params.solution_method = AlignViaTPSParams::SolutionMethod::PseudoInverse;
            }else{
                throw std::runtime_error("Solver not understood. Unable to continue.");
            }

            auto t_opt = AlignViaTPS( params,
                                      (*pcp_it)->pset,
                                      (*ref_PCs.front())->pset );
            if(t_opt){
                FUNCINFO("Successfully found warp using TPS");
                DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( ) );
                DICOM_data.trans_data.back()->transform = t_opt.value();
                DICOM_data.trans_data.back()->metadata["Name"] = "unspecified";
                DICOM_data.trans_data.back()->metadata["WarpType"] = "TPS";
            }else{
                throw std::runtime_error("Failed to warp using TPS-RPM.");
            }

        }else if( std::regex_match(MethodStr, regex_tpsrpm) ){
            auto t_opt = AlignViaTPSRPM( (*pcp_it)->pset,
                                         (*ref_PCs.front())->pset );
            if(t_opt){
                FUNCINFO("Successfully found warp using TPS-RPM");
                DICOM_data.trans_data.emplace_back( std::make_shared<Transform3>( ) );
                DICOM_data.trans_data.back()->transform = t_opt.value();
                DICOM_data.trans_data.back()->metadata["Name"] = "unspecified";
                DICOM_data.trans_data.back()->metadata["WarpType"] = "TPS-RPM";
            }else{
                throw std::runtime_error("Failed to warp using TPS-RPM.");
            }
#endif // DCMA_USE_EIGEN

        }else{
            throw std::invalid_argument("Method not understood. Cannot continue.");
        }

    } // Loop over point clouds.

    return DICOM_data;
}
