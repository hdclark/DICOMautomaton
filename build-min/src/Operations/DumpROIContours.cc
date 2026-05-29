//DumpROIContours.cc - A part of DICOMautomaton 2016. Written by hal clark.

#include <optional>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>
#include <filesystem>
#include <cstdint>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "DumpROIContours.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)



OperationDoc OpArgDocDumpROIContours(){
    OperationDoc out;
    out.name = "DumpROIContours";

    out.tags.emplace_back("category: contour processing");
    out.tags.emplace_back("category: mesh processing");
    out.tags.emplace_back("category: file export");

    out.desc = 
        "This operation exports contours in a standard surface mesh format (structured ASCII Wavefront OBJ)"
        " in planar polygon format. A companion material library file (MTL) assigns colours to each ROI to help"
        " differentiate them.";

    out.notes.emplace_back(
        "Contours that are grouped together into a contour_collection are treated as a logical within the output."
        " For example, all contours in a collection will share a common material property (e.g., colour)."
        " If more fine-grained grouping is required, this routine can be called once for each group which will"
        " result in a logical grouping of one ROI per file."
    );

    out.args.emplace_back();
    out.args.back().name = "DumpFileName";
    out.args.back().desc = "A filename (or full path) in which to (over)write with contour data."
                      " File format is Wavefront obj."
                      " Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile.obj", "localfile.obj", "derivative_data.obj" };
    out.args.back().mimetype = "application/obj";

    out.args.emplace_back();
    out.args.back().name = "MTLFileName";
    out.args.back().desc = "A filename (or full path) in which to (over)write a Wavefront material library file."
                      " This file is used to colour the contours to help differentiate them."
                      " Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/materials.mtl", "localfile.mtl", "somefile.mtl" };
    out.args.back().mimetype = "application/mtl";

    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";

    out.args.emplace_back();
    out.args.back() = CCWhitelistOpArgDoc();
    out.args.back().name = "ROISelection";
    out.args.back().default_val = "all";

    return out;
}



bool DumpROIContours(Drover &DICOM_data,
                       const OperationArgPkg& OptArgs,
                       std::map<std::string, std::string>& /*InvocationMetadata*/,
                       const std::string&){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto DumpFileName = OptArgs.getValueStr("DumpFileName").value();
    auto MTLFileName = OptArgs.getValueStr("MTLFileName").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------

    if(DumpFileName.empty()){
        DumpFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_dumproicontours_", 6, ".obj");
    }
    if(MTLFileName.empty()){
        MTLFileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_dumproicontours_", 6, ".mtl");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }


    //Generate a Wavefront materials file to colour the contours differently.
    std::vector<std::string> mats; // List of materials defined.
    {
        std::ofstream FO(MTLFileName, std::fstream::out);

        const auto write_mat = [&](const std::string &name,
                                   const vec3<double> &c_amb,
                                   const vec3<double> &c_dif,
                                   const vec3<double> &c_spc,
                                   const double s_exp,
                                   const double trans) -> void {

                mats.push_back(name);
                FO << "newmtl " << name << std::endl;
                FO << "Ka " << c_amb.x << " " << c_amb.y << " " << c_amb.z << std::endl; //Ambient colour.
                FO << "Kd " << c_dif.x << " " << c_dif.y << " " << c_dif.z << std::endl; //Diffuse colour.
                FO << "Ks " << c_spc.x << " " << c_spc.y << " " << c_spc.z << std::endl; //Specular colour.
                FO << "Ns " << s_exp << std::endl; //Specular exponent.
                FO << "d " << trans << std::endl; //Transparency ("dissolved"); d=1 is fully opaque.
                FO << "illum 2" << std::endl; //Illumination model (2 = "Colour on, Ambient on").
                FO << std::endl;
        };

        //write_mat("Red", vec3<double>(R, G, B), vec3<double>(R, G, B), vec3<double>(R, G, B), 10.0, 0.9);
        
        //Basic, core colours.
        //write_mat("Red",     vec3<double>(1.0, 0.0, 0.0), vec3<double>(1.0, 0.0, 0.0), vec3<double>(1.0, 0.0, 0.0), 10.0, 0.9);
        //write_mat("Green",   vec3<double>(0.0, 1.0, 0.0), vec3<double>(0.0, 1.0, 0.0), vec3<double>(0.0, 1.0, 0.0), 10.0, 0.9);
        //write_mat("Blue",    vec3<double>(0.0, 0.0, 1.0), vec3<double>(0.0, 0.0, 1.0), vec3<double>(0.0, 0.0, 1.0), 10.0, 0.9);

        // From 'http://stackoverflow.com/questions/470690/how-to-automatically-generate-n-distinct-colors' on 20160919
        // which originate from: Kelly, Kenneth L. "Twenty-two colors of maximum contrast." Color Engineering 3.26 (1965): 26-27.

        write_mat("vivid_yellow", vec3<double>(1.000,0.702,0.000), vec3<double>(1.000,0.702,0.000), vec3<double>(1.000,0.702,0.000), 10.0, 0.9);
        write_mat("strong_purple", vec3<double>(0.502,0.243,0.459), vec3<double>(0.502,0.243,0.459), vec3<double>(0.502,0.243,0.459), 10.0, 0.9);
        write_mat("vivid_orange", vec3<double>(1.000,0.408,0.000), vec3<double>(1.000,0.408,0.000), vec3<double>(1.000,0.408,0.000), 10.0, 0.9);
        write_mat("very_light_blue", vec3<double>(0.651,0.741,0.843), vec3<double>(0.651,0.741,0.843), vec3<double>(0.651,0.741,0.843), 10.0, 0.9);
        write_mat("vivid_red", vec3<double>(0.757,0.000,0.125), vec3<double>(0.757,0.000,0.125), vec3<double>(0.757,0.000,0.125), 10.0, 0.9);
        write_mat("grayish_yellow", vec3<double>(0.808,0.635,0.384), vec3<double>(0.808,0.635,0.384), vec3<double>(0.808,0.635,0.384), 10.0, 0.9);
        write_mat("medium_gray", vec3<double>(0.506,0.439,0.400), vec3<double>(0.506,0.439,0.400), vec3<double>(0.506,0.439,0.400), 10.0, 0.9);
        write_mat("vivid_green", vec3<double>(0.000,0.490,0.204), vec3<double>(0.000,0.490,0.204), vec3<double>(0.000,0.490,0.204), 10.0, 0.9);
        write_mat("strong_purplish_pink", vec3<double>(0.965,0.463,0.557), vec3<double>(0.965,0.463,0.557), vec3<double>(0.965,0.463,0.557), 10.0, 0.9);
        write_mat("strong_blue", vec3<double>(0.000,0.325,0.541), vec3<double>(0.000,0.325,0.541), vec3<double>(0.000,0.325,0.541), 10.0, 0.9);
        write_mat("strong_yellowish_pink", vec3<double>(1.000,0.478,0.361), vec3<double>(1.000,0.478,0.361), vec3<double>(1.000,0.478,0.361), 10.0, 0.9);
        write_mat("strong_violet", vec3<double>(0.325,0.216,0.478), vec3<double>(0.325,0.216,0.478), vec3<double>(0.325,0.216,0.478), 10.0, 0.9);
        write_mat("vivid_orange_yellow", vec3<double>(1.000,0.557,0.000), vec3<double>(1.000,0.557,0.000), vec3<double>(1.000,0.557,0.000), 10.0, 0.9);
        write_mat("strong_purplish_red", vec3<double>(0.702,0.157,0.318), vec3<double>(0.702,0.157,0.318), vec3<double>(0.702,0.157,0.318), 10.0, 0.9);
        write_mat("vivid_greenish_yellow", vec3<double>(0.957,0.784,0.000), vec3<double>(0.957,0.784,0.000), vec3<double>(0.957,0.784,0.000), 10.0, 0.9);
        write_mat("strong_reddish_brown", vec3<double>(0.498,0.094,0.051), vec3<double>(0.498,0.094,0.051), vec3<double>(0.498,0.094,0.051), 10.0, 0.9);
        write_mat("vivid_yellowish_green", vec3<double>(0.576,0.667,0.000), vec3<double>(0.576,0.667,0.000), vec3<double>(0.576,0.667,0.000), 10.0, 0.9);
        write_mat("deep_yellowish_brown", vec3<double>(0.349,0.200,0.082), vec3<double>(0.349,0.200,0.082), vec3<double>(0.349,0.200,0.082), 10.0, 0.9);
        write_mat("vivid_reddish_orange", vec3<double>(0.945,0.227,0.075), vec3<double>(0.945,0.227,0.075), vec3<double>(0.945,0.227,0.075), 10.0, 0.9);
        write_mat("dark_olive_green", vec3<double>(0.137,0.173,0.086), vec3<double>(0.137,0.173,0.086), vec3<double>(0.137,0.173,0.086), 10.0, 0.9);

        FO.flush();
    }

    //Dump the data in a structured ASCII Wavefront OBJ format using native polygons.
    //
    // NOTE: This routine creates a single polygon for each contour. Some programs might not be able to handle this,
    //       and may require triangles or quads at most.
    {
        std::ofstream FO(DumpFileName, std::fstream::out);

        //Reference the MTL file, but use relative paths to make moving files around easier without having to modify them.
        //FO << "mtllib " << MTLFileName << std::endl;
        FO << "mtllib " << SplitStringToVector(MTLFileName, '/', 'd').back() << std::endl;
        FO << std::endl;
 
        int64_t gvc = 0; // Global vertex count. Used to track vert number because they have whole-file scope.
        int64_t family = 0;
        for(auto &cc_ref : cc_ROIs){
            int64_t contour = 0;
            for(auto &c : cc_ref.get().contours){
                const auto N = c.points.size();
                if(N < 3) continue;

                //Generate an object name.
                FO << "o Contour_" << family << "_" << contour++ << std::endl;
                FO << std::endl;

                //Add useful comments, such as ROIName.
                FO << "# Metadata: ROIName = " << c.metadata["ROIName"] << std::endl;
                FO << "# Metadata: NormalizedROIName = " << c.metadata["NormalizedROIName"] << std::endl;

                //Choose a face colour.
                //
                // Note: Simply add more colours above if you need more colours here.
                // Note: The obj format does not support per-vertex colours.
                // Note: The usmtl statement should be before the vertices because some loaders (e.g., Meshlab)
                //       apply the material to vertices instead of faces.
                //
                FO << "usemtl " << mats[ family % mats.size() ] << std::endl;

                //Print the vertices.
                const auto defaultprecision = FO.precision();
                FO << std::setprecision(std::numeric_limits<long double>::max_digits10); 
                for(const auto &p : c.points){
                    FO << "v " << p.x << " " << p.y << " " << p.z << std::endl;
                }
                FO << std::endl;
                FO << std::setprecision(defaultprecision);

                //Print the face linkages. 
                //
                // Note: The obj format starts at 1, not 0. Polygons are implicitly closed and do not need to include a
                //       duplicate vertex.
                // 
                FO << "f";
                for(uint64_t i = 1; i <= N; ++i) FO << " " << (gvc+i);
                //if(c.closed) FO << " " << (gvc+1);
                FO << std::endl; 
                FO << std::endl; 
                gvc += N;
            }
            ++family;
        }

        FO.flush();
    }

    return true;
}
