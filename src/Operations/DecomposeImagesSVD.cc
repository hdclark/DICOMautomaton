//DecomposeImagesSVD.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <cstdint>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorStats.h"

#ifdef DCMA_USE_EIGEN
    #include <eigen3/Eigen/Dense>
    #include <eigen3/Eigen/Eigenvalues>
    #include <eigen3/Eigen/SVD>
    #include <eigen3/Eigen/QR>
    #include <eigen3/Eigen/Cholesky>
#else
    #error "Attempted to compile without Eigen support, which is required."
#endif

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Grouping/Misc_Functors.h"
#include "../YgorImages_Functors/Processing/Partitioned_Image_Voxel_Visitor_Mutator.h"

#include "DecomposeImagesSVD.h"


OperationDoc OpArgDocDecomposeImagesSVD(){
    OperationDoc out;
    out.name = "DecomposeImagesSVD";

    out.desc = 
        "This operation uses Singular Value Decomposition (SVD) on a set of images to generate an orthonormal basis."
        " The basis is ordered and such that the first image corresponds with the largest singular value."
        " The resulting basis can be used for classification, compression, and principal component analysis, among"
        " other things.";

    out.notes.emplace_back(
        "Images are 'reshaped' from a MxN matrix to a vector with length MxN using the default Ygor image pixel"
        " ordering (row-major)."
    );
    out.notes.emplace_back(
        "Spatial information is disregarded for all images, and the basis images have default geometry."
    );

    out.args.emplace_back();
    out.args.back() = IAWhitelistOpArgDoc();
    out.args.back().name = "ImageSelection";
    out.args.back().default_val = "last";

    out.args.emplace_back();
    out.args.back().name = "Channel";
    out.args.back().desc = "The image channel to use. Zero-based. Use '-1' to operate on all available channels.";
    out.args.back().default_val = "-1";
    out.args.back().expected = true;
    out.args.back().examples = { "-1", "0", "1", "2" };

    return out;
}



bool DecomposeImagesSVD(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto ImageSelectionStr = OptArgs.getValueStr("ImageSelection").value();
    const auto Channel = std::stol( OptArgs.getValueStr("Channel").value() );

    //-----------------------------------------------------------------------------------------------------------------

    int64_t rows = -1L;
    int64_t cols = -1L;
    int64_t chns = -1L;
    int64_t imgs =  0L;

    auto IAs_all = All_IAs( DICOM_data );
    auto IAs = Whitelist( IAs_all, ImageSelectionStr );

    // Ensure all images have the same voxel data layout.
    for(const auto & iap_it : IAs){
        for(const auto & img : (*iap_it)->imagecoll.images){
            const auto l_rows = img.rows;
            const auto l_cols = img.columns;
            const auto l_chns = img.channels;

            if(rows < 0) rows = l_rows;
            if(cols < 0) cols = l_cols;
            if(chns < 0) chns = l_chns;

            if( (rows != l_rows)
            ||  (cols != l_cols)
            ||  (chns != l_chns) ){
                throw std::runtime_error("Not all images share the same number of rows, columns, and/or channels");
            }
            ++imgs;
        }
    }
    if(imgs == 0L){
        throw std::invalid_argument("No images selected. Cannot continue");
    }
    if(chns <= Channel){
        throw std::invalid_argument("Requested channel does not exist");
    }

    std::set<int64_t> Channels;
    if(Channel < 0){
        for(int64_t i = 0; i < chns; ++i) Channels.insert(i);
    }else{
        Channels.insert(Channel);
    }

    // Compute the average for every voxel.
    const int64_t N_cols = imgs;
    const int64_t N_rows = rows * cols * Channels.size();

    planar_image<float, double> avg;
    avg.init_buffer(rows, cols, chns);
    for(int64_t r = 0; r < rows; ++r){
        for(int64_t c = 0; c < cols; ++c){
            for(const auto &h : Channels){

                Stats::Running_Sum<double> rs;
                for(const auto & iap_it : IAs){
                    for(const auto & img : (*iap_it)->imagecoll.images){
                        rs.Digest( img.value(r, c, h) );
                    }
                }
                avg.reference(r, c, h) = rs.Current_Sum() / static_cast<double>(imgs);
            }
        }
    }

    // Pack the matrix for SVD decomposition.
    Eigen::MatrixXd X(N_rows, N_cols);
    {
        int64_t i = 0;
        for(const auto & iap_it : IAs){
            for(const auto & img : (*iap_it)->imagecoll.images){
                for(int64_t n = 0L; n < N_rows; ++n){
                    X(n, i) = img.value(n) - avg.value(n);
                }

                //int64_t n = 0;
                //for(int64_t r = 0; r < rows; ++r){
                //    for(int64_t c = 0; c < cols; ++c){
                //        for(const auto &h : Channels){
                //            X(n++, i) = img.value(r, c, h) - avg.value(r, c, h);
                //        }
                //    }
                //}
                ++i;
            }
        }
    }

    YLOGINFO("Performing SVD decomposition on " << N_rows << "x" << N_cols << " matrix now");

    // For the future:
    //using SVD_t = Eigen::BDCSVD<Eigen::MatrixXd, Eigen::ComputeThinU | Eigen::ComputeThinV>;
    //SVD_t SVD;
    //SVD.compute(X);

    // Using deprecated versions currently (at time of writing) present in Debian:
    using SVD_t = Eigen::BDCSVD<Eigen::MatrixXd>;
    SVD_t SVD;
    SVD.compute(X, Eigen::ComputeThinU | Eigen::ComputeThinV );

    // TODO: how do I access the info function??
    //using SVD_base_t = Eigen::EigenBase<SVD_t>; // Needed to access status.
    //auto *SVD_base = reinterpret_cast<SVD_base_t*>(SVD.compute(MST, Eigen::ComputeFullU | Eigen::ComputeFullV ));
    //if(SVD_base->info() != Eigen::ComputationInfo::Success){
    //    throw std::runtime_error("SVD computation failed");
    //}
    const Eigen::MatrixXd &U = SVD.matrixU();
    const Eigen::VectorXd &S = SVD.singularValues();
    const Eigen::MatrixXd &V = SVD.matrixV();

    YLOGINFO("SVD rank: " << SVD.rank());
    YLOGINFO("SVD # of non-zero singular values: " << SVD.nonzeroSingularValues());
    YLOGINFO("Decomposition matrix U has dimensions " << U.rows() << "x" << U.cols());
    YLOGINFO("Decomposition matrix V has dimensions " << V.rows() << "x" << V.cols());
    YLOGINFO("Decomposition vector S has length " << S.size());

    // Create a new image array with the basis images.
    auto out = std::make_unique<Image_Array>();
    const vec3<double> ImageOrientationRow(0.0, 1.0, 0.0);
    const vec3<double> ImageOrientationColumn(1.0, 0.0, 0.0);
    const vec3<double> ImageAnchor(0.0, 0.0, 0.0);
    const vec3<double> ImagePosition(0.0, 0.0, 0.0);
    const int64_t NumberOfRows = rows;
    const int64_t NumberOfColumns = cols;
    const int64_t NumberOfChannels = Channels.size();
    const double VoxelWidth = 1.0;
    const double VoxelHeight = 1.0;
    const double SliceThickness = 1.0;
    {
        const int64_t U_cols = U.cols();
        for(int64_t i = 0L; i < U_cols; ++i){
            out->imagecoll.images.emplace_back();
            auto *img = &(out->imagecoll.images.back());

            // Note: no 'standard' image metadata is assigned here.
            // Not sure if it's needed, especially since the output is a
            // basis, which should be applicable to other coordinate systems, etc.
            img->metadata["SingularValue"] = std::to_string( S(i) );

            img->init_orientation(ImageOrientationRow, ImageOrientationColumn);
            img->init_buffer(NumberOfRows, NumberOfColumns, NumberOfChannels);
            img->init_spatial(VoxelWidth, VoxelHeight, SliceThickness, ImageAnchor, ImagePosition);

            for(int64_t n = 0L; n < N_rows; ++n){
                img->reference(n) = U(n, i);
            }
        }
    }

    if(!out->imagecoll.images.empty()){
        DICOM_data.image_data.emplace_back(std::move(out));
    }

    return true;
}
