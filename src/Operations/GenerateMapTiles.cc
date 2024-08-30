//GenerateMapTiles.cc - A part of DICOMautomaton 2024. Written by hal clark.

#include <algorithm>
#include <functional>
#include <filesystem>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <stdexcept>
#include <string>    
#include <cstdint>

#include "Explicator.h"       //Needed for Explicator class.

#include "YgorMisc.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../Metadata.h"
#include "../File_Loader.h"
#include "../String_Parsing.h"
#include "../GIS.h"


#include "GenerateMapTiles.h"

static
bool
download_url(std::string url, std::filesystem::path p){
    // Attempt to use an external command to download a file from a remote server.
    // It would be better to use a built-in HTTPS-capable client, but that comes with a *lot* of baggage.
    // For now, until this is no longer suitable, shelling out seems like the better option.

    enum class download_method {
        wget,
        curl,
    };

    std::set<download_method> dm;
#if defined(_WIN32) || defined(_WIN64)
    dm.insert( download_method::wget );
    dm.insert( download_method::curl );
#endif
#if defined(__linux__)
    dm.insert( download_method::wget );
    dm.insert( download_method::curl );
#endif
#if defined(__APPLE__) && defined(__MACH__)
    dm.insert( download_method::curl );
#endif

    std::map<std::string, std::string> key_vals;
    //key_vals["USERAGENT"] = escape_for_quotes("Mozilla/5.0 (X11; Linux x86_64; rv:102.0) Gecko/20100101 Firefox/102.0");
    key_vals["USERAGENT"] = escape_for_quotes("DICOMautomaton GenerateMapTiles");
    key_vals["URL"] = escape_for_quotes(url);
    key_vals["DESTFILE"] = escape_for_quotes(p.string());

    bool success = false;
    int64_t tries = 0;
    while((tries++ < 3) && !success){
        try{

            // Wget.
            if(dm.count(download_method::wget) != 0){
                // Build the invocation.
                const std::string proto_cmd = R"***(: | wget --no-verbose -U '$USERAGENT' '$URL' -O '$DESTFILE' && echo 'OK' )***";
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "$");

                // Query the user.
                YLOGDEBUG("About to invoke shell command: '" << cmd << "'");
                auto res = Execute_Command_In_Pipe(cmd);
                res = escape_for_quotes(res); // Trim newlines and unprintable characters.
                YLOGDEBUG("Received response: '" << res << "'");

                // Break out of the while loop on success.
                success = (res == "OK");
                if(success) break;
            }

            // Curl.
            if(dm.count(download_method::curl) != 0){
                // Build the invocation.
                const std::string proto_cmd = R"***(curl --silent --output '$DESTFILE' --user-agent '$USERAGENT' '$URL' && echo 'OK' )***";
                std::string cmd = ExpandMacros(proto_cmd, key_vals, "$");

                // Query the user.
                YLOGDEBUG("About to invoke shell command: '" << cmd << "'");
                auto res = Execute_Command_In_Pipe(cmd);
                res = escape_for_quotes(res); // Trim newlines and unprintable characters.
                YLOGDEBUG("Received response: '" << res << "'");

                // Break out of the while loop on success.
                success = (res == "OK");
                if(success) break;
            }

        }catch(const std::exception &e){
            YLOGWARN("URL download failed: '" << e.what() << "'");
        }

        if(3 <= tries){
            throw std::runtime_error("Unable to download URL");
        }
    }

    return success;
}


OperationDoc OpArgDocGenerateMapTiles(){
    OperationDoc out;
    out.name = "GenerateMapTiles";
    out.desc = 
        "This operation generates an image (representing a geospatial map) from one or more contours"
        " (representing geospatial traces). It can be used to help visualize or analyze tracks and terrain.";

    out.notes.emplace_back(
        "This operation can use a local cache or download tiles from a remote server into a local cache."
        " Maintenance of the cache is left to the user; for one-off invocations, it is recommended to delete"
        " the cache as soon as possible to avoid stale data."
    );
    out.notes.emplace_back(
        "This operation is known to fail when contours traverse the 180 degrees longitude line."
    );

    out.args.emplace_back();
    out.args.back().name = "Zoom";
    out.args.back().desc = "Web Mercator projection zoom parameter. This factor represents an exponent; the resolution"
                           " of the map doubles with each additional zoom factor. Increasing the zoom by one results in"
                           " four times as many tiles needed to cover the same geographical area."
                           "\n\n"
                           "The specific zoom required will depend on the required level of detail, but as a rough"
                           " guide use 1-5 for countries, 5-10 for intracountry states/provinces, 10-15 for"
                           " cites/municipalities, and 15-19 for parks/trails.";
    out.args.back().default_val = "10";
    out.args.back().expected = true;
    out.args.back().examples = { "1", "5", "10", "12", "14", "16" };

    out.args.emplace_back();
    out.args.back().name = "ProviderURL";
    out.args.back().desc = "The prototype URL endpoint used to request map tile images."
                           "\n\n"
                           "This prototype URL assumes a typical format for web Mercator map tile servers."
                           " (This operation is specifically designed to be compatible with OpenStreetMap tile servers,"
                           " but other servers similarly following the osgeo.org Tile Map Server Specification are"
                           " likely to be compatible.)"
                           "\n\n"
                           "Variables within the prototype URL will be replaced for individual tiles."
                           " The following metadata are currently recognized:"
                           " $TILEX (the web Mercator tile coordinate for longitude),"
                           " $TILEY (the web Mercator tile coordinate for latitude), and"
                           " $ZOOM (the web Mercator zoom factor)."
                           "\n\n"
                           "Downloads can be disabled by providing an invalid URL, e.g., '/dev/null'.";
    out.args.back().default_val = "https://tile.openstreetmap.org/${ZOOM}/${TILEX}/${TILEY}.png";
    out.args.back().expected = true;
    out.args.back().examples = { "/dev/null",
                                 "https://tile.openstreetmap.org/${ZOOM}/${TILEX}/${TILEY}.png",
                                 "http://tile.thunderforest.com/cycle/${ZOOM}/${TILEX}/${TILEY}.png?apikey=abc123xyz",
                                 "https://maptiles.p.rapidapi.com/local/osm/v1/${ZOOM}/${TILEX}/${TILEY}.png?rapidapi-key=abc123xyz" };

    out.args.emplace_back();
    out.args.back().name = "LayerName";
    out.args.back().desc = "The name or ID associated with a given tile set."
                           "\n\n"
                           "This name is predominantly used for caching purposes. Each distinct provider should have a"
                           " corresponding distinct LayerName, otherwise tiles from multiple layers will be mixed.";
    out.args.back().default_val = "OSM";
    out.args.back().expected = true;
    out.args.back().examples = { "OSM" };

    const auto swap_backslashes = [](const std::string &s){
        return Lineate_Vector( SplitStringToVector(s, '\\', 'd'), "/" );
    };
    const auto tempdir = swap_backslashes( std::filesystem::temp_directory_path().string() );
    const auto tcd = swap_backslashes( (std::filesystem::temp_directory_path() / "dcma_generatemaptile_cache").string());
    out.args.emplace_back();
    out.args.back().name = "TileCacheDirectory";
    out.args.back().desc = "The top-level directory wherein tiles are, or can be, cached."
                           "\n\n"
                           "The cache structure follows a common hierarchical organization:"
                           " '${TileCacheDirectory}/${LayerName}/${zoom}/${x_tile_number}/${y_tile_number}.png'"
                           "\n\n"
                           "Note: filenames with backslashes ('\\') will need to escape the backslash character, which"
                           " is interpretted as an escape character when parsing operation parameters. Backslashes can"
                           " also be replaced with forwardslahses ('/') in some cases.";
    out.args.back().default_val = tcd;
    out.args.back().expected = true;
    out.args.back().examples = { tempdir, 
                                 ".",
                                 "$HOME/.cache/dcma_map_tiles/" };

    out.args.emplace_back();
    out.args.back().name = "MaxMemory";
    out.args.back().desc = "Abort when the map would exceed this amount of memory (in bytes).";
    out.args.back().default_val = std::to_string(2LL * 1024LL * 1024LL * 1024LL);
    out.args.back().expected = true;
    out.args.back().examples = { "524288000",
                                 "1073741824",
                                 "2147483648" };

    out.args.emplace_back();
    out.args.back().name = "TileWidth";
    out.args.back().desc = "The width, in pixels, of each tile image. Either 256 or 512 is typical.";
    out.args.back().default_val = "256";
    out.args.back().expected = true;
    out.args.back().examples = { "256", "512" };

    out.args.emplace_back();
    out.args.back().name = "TileHeight";
    out.args.back().desc = "The height, in pixels, of each tile image. Either 256 or 512 is typical.";
    out.args.back().default_val = "256";
    out.args.back().expected = true;
    out.args.back().examples = { "256", "512" };

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

bool GenerateMapTiles(Drover &DICOM_data,
                      const OperationArgPkg& OptArgs,
                      std::map<std::string, std::string>& /*InvocationMetadata*/,
                      const std::string& FilenameLex){

    Explicator X(FilenameLex);

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Zoom = static_cast<int64_t>( std::stoll( OptArgs.getValueStr("Zoom").value() ));

    const auto TileCacheDirectoryStr = OptArgs.getValueStr("TileCacheDirectory").value();
    const auto ProviderURLStr = OptArgs.getValueStr("ProviderURL").value();
    const auto LayerNameStr = OptArgs.getValueStr("LayerName").value();

    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto ROISelection = OptArgs.getValueStr("ROISelection").value();

    const auto MaxMemory = std::stol( OptArgs.getValueStr("MaxMemory").value() );
    const auto TileWidth = std::stol( OptArgs.getValueStr("TileWidth").value() );
    const auto TileHeight = std::stol( OptArgs.getValueStr("TileHeight").value() );

    //-----------------------------------------------------------------------------------------------------------------
    YLOGINFO("Proceeding with TileCacheDirectory = '" << TileCacheDirectoryStr << "'");

    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, ROILabelRegex, NormalizedROILabelRegex, ROISelection );
    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    // Assume contours have been Mercator projected so that north is +y, south is -y, west is -x, and east is +x.
    // Find extrema.
    //
    // TODO: account for wrap! The min and max might be geographically adjacent if the contour straddles the
    // longitude = 180 degrees line.
    std::optional<double> min_x, max_x;
    std::optional<double> min_y, max_y;
    for(const auto &cc_refw : cc_ROIs){
        for(const auto &c : cc_refw.get().contours){
            for(const auto &p : c.points){
                if( !min_x || (p.x < min_x.value()) ) min_x = p.x;
                if( !max_x || (max_x.value() < p.x) ) max_x = p.x;
                if( !min_y || (p.y < min_y.value()) ) min_y = p.y;
                if( !max_y || (max_y.value() < p.y) ) max_y = p.y;
            }
        }
    }
    if( !min_x || !max_x || !min_y || !max_y ){
        throw std::invalid_argument("Unable to extract bounding box: insufficient data available");
    }

    const auto [min_lat, min_lon] = dcma::gis::project_inverse_mercator(min_x.value(), min_y.value());
    const auto [max_lat, max_lon] = dcma::gis::project_inverse_mercator(max_x.value(), max_y.value());
    const auto [NW_tile_x, NW_tile_y] = dcma::gis::project_web_mercator(min_lat, min_lon, Zoom);
    const auto [SE_tile_x, SE_tile_y] = dcma::gis::project_web_mercator(max_lat, max_lon, Zoom);
    const auto NS_tile_count = (SE_tile_y - NW_tile_y + 1);
    const auto EW_tile_count = (SE_tile_x - NW_tile_x + 1);
    const auto tile_count = NS_tile_count * EW_tile_count;

    YLOGINFO("Bounding box (lat, lon): (" << min_lat << ", " << min_lon << "), (" << max_lat << ", " << max_lon << ")");
    YLOGINFO("Total required tile count: " << tile_count);
    YLOGDEBUG("north-west tile coords: " << NW_tile_x << ", " << NW_tile_y);
    YLOGDEBUG("south-east tile coords: " << SE_tile_x << ", " << SE_tile_y);

    // Limit the total amount of memory the amalgamated map can consume.
    const auto memory_needed = static_cast<int64_t>(tile_count * TileHeight * TileWidth * 32 * 3);
    if(MaxMemory < memory_needed){
        throw std::runtime_error("The map at current zoom level would consume too much memory."
                                 " Decrease zoom level, decrease field-of-view, or increase memory limit.");
    }
    std::set<std::pair<int64_t, int64_t>> tile_coords;
    for(auto i = NW_tile_x; i <= SE_tile_x; ++i){
        for(auto j = NW_tile_y; j <= SE_tile_y; ++j){
            tile_coords.insert({i, j});
        }
    }

    // Prep the amalgamated image.
    auto out = std::make_unique<Image_Array>();
    out->imagecoll.images.emplace_back();
    auto &img = out->imagecoll.images.back();

    const vec3<double> ImageOrientationRow(1.0, 0.0, 0.0);
    const vec3<double> ImageOrientationColumn(0.0, 1.0, 0.0);
    img.init_orientation(ImageOrientationRow, ImageOrientationColumn);

    const int64_t NumberOfRows = NS_tile_count * TileHeight;
    const int64_t NumberOfColumns = EW_tile_count * TileWidth;
    const int64_t NumberOfChannels = 3;
    YLOGINFO("Creating image with " << NumberOfRows << "x" << NumberOfColumns << " pixels requiring " << memory_needed << " bytes");
    img.init_buffer(NumberOfRows, NumberOfColumns, NumberOfChannels);

    // In order to get the pixel height and width, we take the total length spanned by the tile and divide by the
    // number of along an edge. Due to projection distortion, this is NOT accurate for large distances since some
    // pixels will be more distorted than others. However, for small distances the difference should be manageable.
    // Regardless, as long as both contours and maps are distorted the same, at least the map will be consistent.
    const auto [NW_tile_lat, NW_tile_lon] = dcma::gis::project_inverse_web_mercator(NW_tile_x, NW_tile_y, Zoom);
    const auto [NWp_tile_lat, NWp_tile_lon] = dcma::gis::project_inverse_web_mercator(NW_tile_x+1, NW_tile_y+1, Zoom);

    const auto [NW_tile_pos_x, NW_tile_pos_y] = dcma::gis::project_mercator(NW_tile_lat, NW_tile_lon);
    const auto [NWp_tile_pos_x, NWp_tile_pos_y] = dcma::gis::project_mercator(NWp_tile_lat, NWp_tile_lon);

    const auto VoxelWidth = std::abs(NWp_tile_pos_x - NW_tile_pos_x) / static_cast<double>(TileWidth);
    const auto VoxelHeight = std::abs(NWp_tile_pos_y - NW_tile_pos_y) / static_cast<double>(TileHeight);
    const auto SliceThickness = 1.0;
    const vec3<double> ImageAnchor(0.0, 0.0, 0.0);
    const vec3<double> ImagePosition(NW_tile_pos_x, NW_tile_pos_y, 0.0);
    img.init_spatial(VoxelWidth, VoxelHeight, SliceThickness, ImageAnchor, ImagePosition);

    // Fill in the pixels with a default value.
    const auto VoxelValue = 0.0f;
    img.fill_pixels(VoxelValue);

    // Transfer pixels from the tiles to the amalgamated image.
    for(const auto& [tile_x, tile_y] : tile_coords){
        auto f = std::filesystem::path(TileCacheDirectoryStr) / LayerNameStr
                                                              / std::to_string(Zoom)
                                                              / std::to_string(tile_x)
                                                              / std::to_string(tile_y);
        f.replace_extension(".png");
        if( ! std::filesystem::exists(f) ){
            // Attempt to insert the tile into the cache.
            YLOGINFO("Tile '" + f.string() + "' was not found in the cache. Attempting to download.");

            // Build the URL for this tile.
            std::map<std::string, std::string> key_vals;
            key_vals["ZOOM"] = escape_for_quotes(std::to_string(Zoom));
            key_vals["TILEX"] = escape_for_quotes(std::to_string(tile_x));
            key_vals["TILEY"] = escape_for_quotes(std::to_string(tile_y));

            // Build the invocation.
            const auto url = ExpandMacros(ProviderURLStr, key_vals, "$");

            const auto dirs = f.parent_path();
            if( !std::filesystem::exists(dirs) ){
                std::filesystem::create_directories(dirs);
            }
            if( !std::filesystem::exists(dirs)
            ||  !std::filesystem::is_directory(dirs) ){
                YLOGWARN("Unable to create cache directory");
            }

            YLOGDEBUG("Attempting to download tile from '" << url << "'");
            if( !download_url(url, f) ){
                YLOGWARN("Unable to download tile");
            }
        }

        if( std::filesystem::exists(f) ){
            YLOGDEBUG("Tile '" + f.string() + "' located in the cache. Attempting to load it.");
            Drover l_DICOM_data;
            std::map<std::string,std::string> l_InvocationMetadata;
            std::list<OperationArgPkg> l_Operations;
            std::list<std::filesystem::path> pl = { f };
            if( !Load_Files(l_DICOM_data, l_InvocationMetadata, FilenameLex, l_Operations, pl)
            ||  !l_DICOM_data.Has_Image_Data() ){
                throw std::runtime_error("Unable to load tile '" + f.string() + "'");
            }

            auto &l_img = l_DICOM_data.image_data.back()->imagecoll.images.back();

            if( (l_img.rows != TileHeight)
            ||  (l_img.columns != TileWidth) ){
                YLOGWARN("Unexpected tile dimensions");
            }
            l_img.apply_to_pixels([&](int64_t l_row, int64_t l_col, int64_t l_chn, float l_val) -> void {
                const auto col = l_col + (tile_x - NW_tile_x) * static_cast<double>(TileWidth);
                const auto row = l_row + (tile_y - NW_tile_y) * static_cast<double>(TileHeight);
                if( (l_col < TileWidth)
                &&  (l_row < TileHeight)
                &&  (l_chn < NumberOfChannels)
                &&  (0 <= row) && (row < NumberOfRows)
                &&  (0 <= col) && (col < NumberOfColumns) ){
                    img.reference(row, col, l_chn) = l_val;
                }
                return;
            });

        }
    }

    DICOM_data.image_data.emplace_back(std::move(out));

    return true;
}
