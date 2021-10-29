//ConvertWarpToMeshes.cc - A part of DICOMautomaton 2021. Written by hal clark.

#include <list>
#include <map>
#include <memory>
#include <string>    

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Alignment_Rigid.h"
#include "../Alignment_TPSRPM.h"
#include "../Alignment_Field.h"

#include "ConvertWarpToMeshes.h"


OperationDoc OpArgDocConvertWarpToMeshes(){
    OperationDoc out;
    out.name = "ConvertWarpToMeshes";

    out.desc = 
        "This operation attempts to convert a warp (i.e., a spatial registration or deformable spatial registration)"
        " to a mesh suitable for viewing or inspecting the geometry.";

    out.args.emplace_back();
    out.args.back() = T3WhitelistOpArgDoc();
    out.args.back().name = "TransformSelection";
    out.args.back().default_val = "last";
    out.args.back().desc = "The transformation that will be exported. "_s
                         + out.args.back().desc;

    out.args.emplace_back();
    out.args.back().name = "VoxelCadence";
    out.args.back().desc = "The number of voxels to skip over. This can be used to reduce the number of triangles"
                           " in the resulting mesh. Prefer prime numbers distant to the number of rows, columns,"
                           " images, and multiples of all three to minimize bunching/clustering. Set to negative"
                           " or zero to display all voxels.";
    out.args.back().default_val = "7";
    out.args.back().expected = true;
    out.args.back().examples = { "0", "7", "71", "197", "313", "971", "1663", "3739" };

    return out;
}

bool ConvertWarpToMeshes(Drover &DICOM_data,
                          const OperationArgPkg& OptArgs,
                          const std::map<std::string, std::string>&,
                          const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto TFormSelectionStr = OptArgs.getValueStr("TransformSelection").value();

    const auto VoxelCadence = std::stol(OptArgs.getValueStr("VoxelCadence").value() );
    //-----------------------------------------------------------------------------------------------------------------

    auto T3s_all = All_T3s( DICOM_data );
    auto T3s = Whitelist( T3s_all, TFormSelectionStr );
    FUNCINFO(T3s.size() << " transformations selected");

    for(auto & t3p_it : T3s){
        std::visit([&](auto && t){
            using V = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<V, std::monostate>){
                throw std::invalid_argument("Transformation is invalid. Unable to continue.");

            // Affine transformations.
            }else if constexpr (std::is_same_v<V, affine_transform<double>>){
                FUNCINFO("Exporting affine transformation now");
                std::runtime_error("Not yet implemented");

            // Thin-plate spline transformations.
            }else if constexpr (std::is_same_v<V, thin_plate_spline>){
                FUNCINFO("Exporting thin-plate spline transformation now");
                std::runtime_error("Not yet implemented");

            // Vector deformation fields.
            }else if constexpr (std::is_same_v<V, deformation_field>){
                FUNCINFO("Exporting vector deformation field now");

                auto out = std::make_unique<Surface_Mesh>();
                long int voxel = 0;
                for(const auto &img : t.field.images){
                    const auto N_rows = img.rows;
                    const auto N_cols = img.columns;

                    const auto N_chns = img.channels;
                    if(N_chns != 3L) throw std::runtime_error("Vector deformation grid does not have three channels");

                    const auto pxl_l = std::max( 0.15 * std::min({ img.pxl_dx, img.pxl_dy, img.pxl_dz }), 1.0E-3 );
                    const auto ortho_unit = img.col_unit.Cross(img.row_unit).unit();

                    for(long int row = 0; row < N_rows; ++row){
                        for(long int col = 0; col < N_cols; ++col){
                            if( (0 < VoxelCadence)
                            &&  ((voxel++ % VoxelCadence) != 0) ) continue;

                            const auto R = img.position(row, col);
                            const vec3<double> dR( img.value(row, col, 0),
                                                   img.value(row, col, 1),
                                                   img.value(row, col, 2) );
                            const auto arrow_length = dR.length();
                            if(arrow_length <= 0.0) continue;

                            // Draw a pyramid shape, but pivot the base to be orthogonal to the arrow direction and also
                            // try align the base's orientation in a consistent way.
                            //
                            // Note: We try to mitigate degeneracy by avoiding likely, grid-aligned starting vectors.
                            //       It's still possible to encounter degeneracy here though.
                            auto axis_1 = dR.unit();
                            if(!axis_1.isfinite()) axis_1 = ortho_unit;
                            auto axis_2 = (img.col_unit * 5.0 + img.row_unit * 1.0 + ortho_unit * 0.1).unit();
                            auto axis_3 = (img.col_unit * 1.0 - img.row_unit * 5.0 - ortho_unit * 0.1).unit();
                            if(!axis_1.GramSchmidt_orthogonalize(axis_2, axis_3)){
                                FUNCWARN("Encountered degeneracy. Skipping element");
                                continue;
                            }
                            axis_1 = axis_1.unit();
                            axis_2 = axis_2.unit();
                            axis_3 = axis_3.unit();

                            // Ensure right-handed arrangement.
                            if(axis_2.Cross(axis_3).Dot(axis_1) < 0.0) axis_3 = axis_3 * (-1.0);

                            const auto N_verts = out->meshes.vertices.size();
                            out->meshes.vertices.emplace_back(R - axis_2 * pxl_l - axis_3 * pxl_l);
                            out->meshes.vertices.emplace_back(R + axis_2 * pxl_l - axis_3 * pxl_l);
                            out->meshes.vertices.emplace_back(R + axis_2 * pxl_l + axis_3 * pxl_l);
                            out->meshes.vertices.emplace_back(R - axis_2 * pxl_l + axis_3 * pxl_l);
                            out->meshes.vertices.emplace_back(R + axis_1 * dR.length());

                            const uint64_t i_A = N_verts + 0;
                            const uint64_t i_B = N_verts + 1;
                            const uint64_t i_C = N_verts + 2;
                            const uint64_t i_D = N_verts + 3;
                            const uint64_t i_E = N_verts + 4;
                            out->meshes.faces.emplace_back(std::vector<uint64_t>{{ i_A, i_B, i_E }});
                            out->meshes.faces.emplace_back(std::vector<uint64_t>{{ i_B, i_C, i_E }});
                            out->meshes.faces.emplace_back(std::vector<uint64_t>{{ i_C, i_D, i_E }});
                            out->meshes.faces.emplace_back(std::vector<uint64_t>{{ i_D, i_A, i_E }});
                            out->meshes.faces.emplace_back(std::vector<uint64_t>{{ i_A, i_D, i_C }});
                            out->meshes.faces.emplace_back(std::vector<uint64_t>{{ i_A, i_C, i_B }});
                        }
                    }
                }
                //out->meshes.metadata["..."] = "...";
                DICOM_data.smesh_data.push_back( std::move( out ) );

            }else{
                static_assert(std::is_same_v<V,void>, "Transformation not understood.");
            }
            return;
        }, (*t3p_it)->transform);
    }
 

    return true;
}
