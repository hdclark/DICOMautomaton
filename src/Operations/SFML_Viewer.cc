//SFML_Viewer.cc - A part of DICOMautomaton 2015 - 2017. Written by hal clark.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>            //Needed for exit() calls.
#include <exception>
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <tuple>
#include <type_traits>
#include <utility>            //Needed for std::pair.
#include <vector>

#include <boost/filesystem.hpp>

#include <SFML/Graphics.hpp>

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Explicator.h"

#include "../Colour_Maps.h"
#include "../Common_Boost_Serialization.h"
#include "../Common_Plotting.h"

#ifdef DCMA_USE_GNU_GSL
    #include "../KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
    #include "../KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
    #include "../KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"
#endif

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"

#include "../Font_DCMA_Minimal.h"

#include "SFML_Viewer.h"


OperationDoc OpArgDocSFML_Viewer(){
    OperationDoc out;
    out.name = "SFML_Viewer";
    out.desc = 
        "Launch an interactive viewer based on SFML."
        " Using this viewer, it is possible to contour ROIs,"
        " generate plots of pixel intensity along profiles or through time,"
        " inspect and compare metadata,"
        " and various other things.";


    out.args.emplace_back();
    out.args.back().name = "SingleScreenshot";
    out.args.back().desc = "If 'true', a single screenshot is taken and then the viewer is exited."
                           " This option works best for quick visual inspections, and should not be"
                           " used for later processing or analysis.";
    out.args.back().default_val = "false";
    out.args.back().expected = true;
    out.args.back().examples = { "true", "false" };

    out.args.emplace_back();
    out.args.back().name = "SingleScreenshotFileName";
    out.args.back().desc = "Iff invoking the 'SingleScreenshot' argument, use this string as the screenshot filename."
                           " If blank, a filename will be generated sequentially.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/a_screenshot.png", "afile.png" };
    out.args.back().mimetype = "image/png";

    out.args.emplace_back();
    out.args.back().name = "FPSLimit";
    out.args.back().desc = "The upper limit on the frame rate, in seconds as an unsigned integer."
                           " Note that this value may be treated as a suggestion.";
    out.args.back().default_val = "60";
    out.args.back().expected = true;
    out.args.back().examples = { "60", "30", "10", "1" };

    return out;
}

Drover SFML_Viewer( Drover DICOM_data, 
                    OperationArgPkg OptArgs, 
                    std::map<std::string,std::string> /*InvocationMetadata*/, 
                    std::string FilenameLex ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto SingleScreenshotStr = OptArgs.getValueStr("SingleScreenshot").value();
    const auto SingleScreenshotFileName = OptArgs.getValueStr("SingleScreenshotFileName").value();
    const auto FPSLimit = std::stoul( OptArgs.getValueStr("FPSLimit").value() );

    //-----------------------------------------------------------------------------------------------------------------
    const auto TrueRegex = Compile_Regex("^tr?u?e?$");

    const auto SingleScreenshot = std::regex_match(SingleScreenshotStr, TrueRegex);
    long int SingleScreenshotCounter = 3; // Used to count down frames before taking the snapshot.

    Explicator X(FilenameLex);


    //Trim any empty image sets.
    for(auto it = DICOM_data.image_data.begin(); it != DICOM_data.image_data.end();  ){
        if((*it)->imagecoll.images.empty()){
            it = DICOM_data.image_data.erase(it);
        }else{
            ++it;
        }
    }
    if(DICOM_data.image_data.empty()) throw std::invalid_argument("No image data available to view. Cannot continue");

    //If, for some reason, several image arrays are available for viewing, we need to provide a means for stepping
    // through the arrays. 
    //
    // NOTE: The reasoning for having several image arrays is not clear cut. If the timestamps are known exactly, it
    //       might make sense to split in this way. In general, it is up to the user to make this call. 
    using img_data_ptr_it_t = decltype(DICOM_data.image_data.begin());
    auto img_array_ptr_beg  = DICOM_data.image_data.begin();
    auto img_array_ptr_end  = DICOM_data.image_data.end();
    auto img_array_ptr_last = std::prev(DICOM_data.image_data.end());
    auto img_array_ptr_it   = img_array_ptr_beg;

    //At the moment, we keep a single 'display' image active at a time. To help walk through the neighbouring images
    // (and the rest of the images, for that matter) we keep a container iterator to the image.
    using disp_img_it_t = decltype(DICOM_data.image_data.front()->imagecoll.images.begin());
    auto disp_img_beg  = (*img_array_ptr_it)->imagecoll.images.begin();
    //disp_img_it_t disp_img_end  = (*img_array_ptr_it)->imagecoll.images.end();  // Not used atm.
    auto disp_img_last = std::prev((*img_array_ptr_it)->imagecoll.images.end());
    auto disp_img_it   = disp_img_beg;

    //Because SFML requires us to keep a sf::Texture alive for the duration of a sf::Sprite, we simply package them
    // together into a texture-sprite bundle. This means a single sf::Sprite must be accompanied by a sf::Texture, 
    // though in general several sf::Sprites could be bound to a sf::Texture.
    typedef std::pair<sf::Texture,sf::Sprite> disp_img_texture_sprite_t;
    disp_img_texture_sprite_t disp_img_texture_sprite;

    //Real-time modifiable sticky window and level.
    std::optional<double> custom_width;
    std::optional<double> custom_centre;

    //A tagged point for measuring distance.
    std::optional<vec3<double>> tagged_pos;

    //Flags for various things.
    bool DumpScreenshot = false; //One-shot instruction to dump a screenshot immediately after rendering.
    bool OnlyShowTagsDifferentToNeighbours = true;
    bool ShowExistingContours = true; //Can be disabled/toggled for, e.g., blind re-contouring.

    //Accumulation-type storage.
    contours_with_meta contour_coll_shtl; //Stores contours in the DICOM coordinate system.
    contour_coll_shtl.contours.emplace_back();    //Prime the shuttle with an empty contour.
    contour_coll_shtl.contours.back().closed = true;

    //Open a window.
    sf::RenderWindow window;
    window.create(sf::VideoMode(640, 480), "DICOMautomaton Image Viewer");
    window.setFramerateLimit(FPSLimit);

    //Create a secondary plotting window. Gets opened on command.
    sf::RenderWindow plotwindow;
    typedef enum {
        None,
        TimeCourse,
        RowProfile,
        ColumnProfile,
    } SecondaryPlot;
    auto plotwindowtype = SecondaryPlot::None;
    if(auto ImageDesc = disp_img_it->GetMetadataValueAs<std::string>("Description")){
        window.setTitle("DICOMautomaton IV: '"_s + ImageDesc.value() + "'");
    }else{
        window.setTitle("DICOMautomaton IV: <no description available>");
    }

    //Attempt to load fonts. We should try a few different files, and include a back-up somewhere accessible...
    sf::Font afont;
    if( !afont.loadFromFile("dcma_minimal.otf") // A minimal ASCII-only font.
    &&  !afont.loadFromFile("/usr/share/fonts/TTF/cmr10.ttf") // Arch Linux 'ttf-computer-modern-fonts' pkg.
    &&  !afont.loadFromFile("/usr/share/fonts/truetype/cmu/cmunrm.ttf") // Debian 'fonts-cmu' pkg.
    &&  !afont.loadFromFile("/usr/share/fonts/gnu-free/FreeMono.otf") // Arch Linux 'gnu-free-fonts' pkg.
    &&  !afont.loadFromFile("/usr/share/fonts/truetype/freefont/FreeMono.ttf") ){ // Debian 'fonts-freefont-ttf' pkg.
        FUNCWARN("Unable to find a suitable font file on host system -- loading embedded minimal font");
        if(!afont.loadFromMemory(static_cast<void*>(dcma_minimal_ttf), static_cast<size_t>(dcma_minimal_ttf_len))){
            FUNCERR("Unable to load embedded font. Cannot continue");
        }
    }

    //Create some primitive shapes, textures, and text objects for display later.
    sf::CircleShape smallcirc(10.0f);
    smallcirc.setFillColor(sf::Color::Transparent);
    smallcirc.setOutlineColor(sf::Color::Green);
    smallcirc.setOutlineThickness(1.0f);

    bool drawcursortext = false; //Usually gets in the way. Sort of a debug feature...
    sf::Text cursortext;
    cursortext.setFont(afont);
    cursortext.setString("");
    cursortext.setCharacterSize(15); //Size in pixels, not in points.
    cursortext.setFillColor(sf::Color::Green);
    cursortext.setOutlineColor(sf::Color::Green);

    sf::Text BRcornertext;
    std::stringstream BRcornertextss;
    BRcornertext.setFont(afont);
    BRcornertext.setString("");
    BRcornertext.setCharacterSize(9); //Size in pixels, not in points.
    BRcornertext.setFillColor(sf::Color::Red);
    BRcornertext.setOutlineColor(sf::Color::Red);

    sf::Text BLcornertext;
    std::stringstream BLcornertextss;
    BLcornertext.setFont(afont);
    BLcornertext.setString("");
    BLcornertext.setCharacterSize(15); //Size in pixels, not in points.
    BLcornertext.setFillColor(sf::Color::Blue);
    BLcornertext.setOutlineColor(sf::Color::Blue);

    const sf::Color NaN_Color(60,0,0); //Dark red. Should not be very distracting.
    const sf::Color Pos_Contour_Color(sf::Color::Blue);
    const sf::Color Neg_Contour_Color(sf::Color::Red);
    const sf::Color Editing_Contour_Color(255,121,0); //"Vivid Orange."

    //Load available colour maps.
    std::vector< std::pair<std::string, std::function<struct ClampedColourRGB(double)>>> colour_maps = {
        std::make_pair("Viridis", ColourMap_Viridis),
        std::make_pair("Magma", ColourMap_Magma),
        std::make_pair("Plasma", ColourMap_Plasma),
        std::make_pair("Inferno", ColourMap_Inferno),

        std::make_pair("Jet", ColourMap_Jet),

        std::make_pair("MorelandBlueRed", ColourMap_MorelandBlueRed),

        std::make_pair("MorelandBlackBody", ColourMap_MorelandBlackBody),
        std::make_pair("MorelandExtendedBlackBody", ColourMap_MorelandExtendedBlackBody),
        std::make_pair("KRC", ColourMap_KRC),
        std::make_pair("ExtendedKRC", ColourMap_ExtendedKRC),

        std::make_pair("Kovesi_LinKRYW_5-100_c64", ColourMap_Kovesi_LinKRYW_5_100_c64),
        std::make_pair("Kovesi_LinKRYW_0-100_c71", ColourMap_Kovesi_LinKRYW_0_100_c71),

        std::make_pair("Kovesi_Cyclic_cet-c2", ColourMap_Kovesi_Cyclic_mygbm_30_95_c78),

        std::make_pair("LANLOliveGreentoBlue", ColourMap_LANL_OliveGreen_to_Blue),

        std::make_pair("YgorIncandescent", ColourMap_YgorIncandescent),

        std::make_pair("LinearRamp", ColourMap_Linear),

        std::make_pair("Composite_50_90_107_110", ColourMap_Composite_50_90_107_110),
        std::make_pair("Composite_50_90_100_107_110", ColourMap_Composite_50_90_100_107_110)
    };
    size_t colour_map = 0;

    const auto load_img_texture_sprite = [&](const disp_img_it_t &img_it, disp_img_texture_sprite_t &out) -> bool {
        //This routine returns a pair of (texture,sprite) because the texture must be kept around
        // for the duration of the sprite.
        const auto img_cols = img_it->columns;
        const auto img_rows = img_it->rows;

        if(!isininc(1,img_rows,10000) || !isininc(1,img_cols,10000)){
            FUNCERR("Image dimensions are not reasonable. Is this a mistake? Refusing to continue");
        }

        sf::Image animage;
        animage.create(img_cols, img_rows);

        //------------------------------------------------------------------------------------------------
        //Apply a window to the data if it seems like the WindowCenter or WindowWidth specified in the image metadata
        // are applicable. Note that it is likely that pixels will be clipped or truncated. This is intentional.
        
        auto img_win_valid = img_it->GetMetadataValueAs<std::string>("WindowValidFor");
        auto img_desc      = img_it->GetMetadataValueAs<std::string>("Description");
        auto img_win_c     = img_it->GetMetadataValueAs<double>("WindowCenter");
        auto img_win_fw    = img_it->GetMetadataValueAs<double>("WindowWidth"); //Full width or range. (Diameter, not radius.)

        auto custom_win_c  = custom_centre; 
        auto custom_win_fw = custom_width; 

        const auto UseCustomWL = (custom_win_c && custom_win_fw);
        const auto UseImgWL = (UseCustomWL) ? false 
                                            : (  img_win_valid && img_desc && img_win_c 
                                              && img_win_fw && (img_win_valid.value() == img_desc.value()));

        if( UseCustomWL || UseImgWL ){
            //Window/linear scaling transformation parameters.
            //const auto win_r = 0.5*(win_fw.value() - 1.0); //The 'radius' of the range, or half width omitting the centre point.

            //The 'radius' of the range, or half width omitting the centre point.
            const auto win_r  = (UseCustomWL) ? 0.5*custom_win_fw.value()
                                              : 0.5*img_win_fw.value();
            const auto win_c  = (UseCustomWL) ? custom_win_c.value()
                                              : img_win_c.value();
            const auto win_fw = (UseCustomWL) ? custom_win_fw.value()
                                              : img_win_fw.value();

            //The output range we are targeting. In this case, a commodity 8 bit (2^8 = 256 intensities) display.
            const auto destmin = static_cast<double>( 0 );
            const auto destmax = static_cast<double>( std::numeric_limits<uint8_t>::max() );
    
            for(auto i = 0; i < img_cols; ++i){
                for(auto j = 0; j < img_rows; ++j){
                    const auto val = static_cast<double>( img_it->value(j,i,0) ); //The first (R or gray) channel.
                    if(!std::isfinite(val)){
                        animage.setPixel(i,j,NaN_Color);
                    }else{
                        double x; // range = [0,1].
                        if(val <= (win_c - win_r)){
                            x = 0.0;
                        }else if(val >= (win_c + win_r)){
                            x = 1.0;
                        }else{
                            x = (val - (win_c - win_r)) / win_fw;
                        }

                        const auto res = colour_maps[colour_map].second(x);
                        const double x_R = res.R;
                        const double x_G = res.G;
                        const double x_B = res.B;

                        const double out_R = x_R * (destmax - destmin) + destmin;
                        const double out_B = x_B * (destmax - destmin) + destmin;
                        const double out_G = x_G * (destmax - destmin) + destmin;
    
                        const auto scaled_R = static_cast<uint8_t>( std::floor(out_R) );
                        const auto scaled_G = static_cast<uint8_t>( std::floor(out_G) );
                        const auto scaled_B = static_cast<uint8_t>( std::floor(out_B) );
                        animage.setPixel(i,j,sf::Color(scaled_R,scaled_G,scaled_B));
                    }
                }
            }

        //------------------------------------------------------------------------------------------------
        //Scale pixels to fill the maximum range. None will be clipped or truncated.
        }else{
            //Due to a strange dependence on windowing, some manufacturers spit out massive pixel values.
            // If you don't want to window you need to anticipate and ignore the gigantic numbers being 
            // you might encounter. This is not the place to do this! If you need to do it here, write a
            // filter routine and *call* it from here.
            //
            // NOTE: This routine could definitely use a re-working, especially to make it safe for all
            //       arithmetical types (i.e., handling negatives, ensuring there is no overflow or wrap-
            //       around, ensuring there is minimal precision loss).
            using pixel_value_t = decltype(img_it->value(0, 0, 0));
            const auto pixel_minmax_allchnls = img_it->minmax();
            const auto lowest = std::get<0>(pixel_minmax_allchnls);
            const auto highest = std::get<1>(pixel_minmax_allchnls);

            //const auto lowest = Stats::Percentile(img_it->data, 0.01);
            //const auto highest = Stats::Percentile(img_it->data, 0.99);

            const auto pixel_type_max = static_cast<double>(std::numeric_limits<pixel_value_t>::max());
            const auto pixel_type_min = static_cast<double>(std::numeric_limits<pixel_value_t>::min());
            const auto dest_type_max = static_cast<double>(std::numeric_limits<uint8_t>::max()); //Min is implicitly 0.

            const double clamped_low  = static_cast<double>(lowest )/pixel_type_max;
            const double clamped_high = static_cast<double>(highest)/pixel_type_max;
    
            for(auto i = 0; i < img_cols; ++i){ 
                for(auto j = 0; j < img_rows; ++j){ 
                    const auto val = img_it->value(j,i,0);
                    if(!std::isfinite(val)){
                        animage.setPixel(i,j,NaN_Color);
                    }else{
                        const double clamped_value = (static_cast<double>(val) - pixel_type_min)/(pixel_type_max - pixel_type_min);
                        auto rescaled_value = (clamped_value - clamped_low)/(clamped_high - clamped_low);
                        if(false){
                        }else if( rescaled_value < 0.0 ){
                            rescaled_value = 0.0;
                        }else if( rescaled_value > 1.0 ){
                            rescaled_value = 1.0;
                        }

                        const auto res = colour_maps[colour_map].second(rescaled_value);
                        const double x_R = res.R;
                        const double x_G = res.G;
                        const double x_B = res.B;
                        
                        const auto scaled_R = static_cast<uint8_t>(x_R * dest_type_max);
                        const auto scaled_G = static_cast<uint8_t>(x_G * dest_type_max);
                        const auto scaled_B = static_cast<uint8_t>(x_B * dest_type_max);
                        animage.setPixel(i,j,sf::Color(scaled_R,scaled_G,scaled_B));
                    }
                }
            }
        }
        

        out.first = sf::Texture();
        out.second = sf::Sprite();
        if(!out.first.create(img_cols, img_rows)) FUNCERR("Unable to create empty SFML texture");
        if(!out.first.loadFromImage(animage)) FUNCERR("Unable to create SFML texture from planar_image");
        //out.first.setSmooth(true);        
        out.first.setSmooth(false);        
        out.second.setTexture(out.first);


        //Scale the displayed pixel aspect ratio if the image pxl_dx and pxl_dy differ.
        {
            const auto ImagePixelAspectRatio = img_it->pxl_dy / img_it->pxl_dx;
            out.second.setScale(1.0f,ImagePixelAspectRatio);
        }

        return true;
    };

    //Scale the image to fill the available space.
    const auto scale_sprite_to_fill_screen = [](const sf::RenderWindow &awindow, 
                                                const disp_img_it_t &img_it, 
                                                disp_img_texture_sprite_t &asprite) -> void {
        //Scale the displayed pixel aspect ratio if the image pxl_dx and pxl_dy differ.
        {
            const auto ImagePixelAspectRatio = img_it->pxl_dx / img_it->pxl_dy;
            asprite.second.setScale(1.0f,ImagePixelAspectRatio);
        }


        //Get the current bounding box size in 'global' coordinates.
        sf::FloatRect img_bb = asprite.second.getGlobalBounds();

        //Get the current window's view's (aka the camera's) viewport coordinates.
        sf::IntRect win_bb = awindow.getViewport( awindow.getView() );

        //Determine how much we can scale the image while keeping it visible.
        // We also need to keep the aspect ratio the same.
        float w_scale = 1.0;
        float h_scale = 1.0;
        h_scale = static_cast<float>(win_bb.height) / img_bb.height;
        w_scale = static_cast<float>(win_bb.width) / img_bb.width;
        h_scale = std::min(h_scale,w_scale);
        w_scale = std::min(h_scale,w_scale);

        //Actually scale the image.
        asprite.second.scale(w_scale,h_scale);
        return;
    };

    //Prep the first image.
    if(!load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
        FUNCERR("Unable to load image --> texture --> sprite");
    }
    scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);


    // Routine for converting the current mouse position to a position in the display image's frame, if possible.
    struct Mouse_Positions {
        // SFML-provided window pixel position.
        bool window_pos_valid = false;
        long int window_pos_row;
        long int window_pos_col;

        // OpenGL (2D) 'world' coordinates.
        bool world_pos_valid = false;
        vec2<float> world_pos;

        // Clamped image position in the image coordinate system (as fractional row and column numbers), iff the mouse
        // is hovering over an image. These are useful for in-plane voxel value interpolation.
        bool clamped_image_pos_valid = false;
        vec2<float> clamped_image_pos; // = vec2<float>(std::numeric_limits<float>::quiet_NaN(),
                                       //             std::numeric_limits<float>::quiet_NaN());

        // Image positions in the image coordinate system (as pixel row and column numbers), iff the mouse is hovering
        // over an image. These are useful for querying voxel values.
        bool pixel_image_pos_valid = false;
        long int pixel_image_pos_row;
        long int pixel_image_pos_col;

        // DICOM position of the mouse, assuming the mouse lies in the image plane.
        bool mouse_DICOM_pos_valid = false;
        vec3<double> mouse_DICOM_pos;

        // DICOM position of the centre of the voxel being hovered over, assuming the mouse lies in the image plane.
        bool voxel_DICOM_pos_valid = false;
        vec3<double> voxel_DICOM_pos;
    };

    // Convert from SFML mouse coordinates to DICOM coordinates using the current display image.
    // 
    // Not all conversions will be possible, so verify each set of coordinates are available prior to using them.
    // However, coordinates are converted sequentially, so if a later coordinate pair are valid then all preceeding
    // coordinate pairs will also be valid.
    //
    // Note: This routine assumes the image is neither rotated nor skewed. It will handle scaled and offset (i.e.,
    // translated) images though.
    const auto Convert_Mouse_Coords = [&]() -> Mouse_Positions {
        Mouse_Positions out;

        // Get the *realtime* current mouse coordinates.
        const sf::Vector2i MousePosWindow = sf::Mouse::getPosition(window);
        out.window_pos_row = MousePosWindow.x;
        out.window_pos_col = MousePosWindow.y;
        out.window_pos_valid = true;

        // Convert to SFML/OpenGL "World" coordinates. 
        sf::Vector2f MousePosWorld = window.mapPixelToCoords(MousePosWindow);
        out.world_pos.x = MousePosWorld.x;
        out.world_pos.y = MousePosWorld.y;
        out.world_pos_valid = true;

        // Check if the mouse is within the image bounding box. We can only proceed if it is.
        sf::FloatRect ImgBBox = disp_img_texture_sprite.second.getGlobalBounds();
        if(!ImgBBox.contains(MousePosWorld)) return out;

        // Determine which image pixel we are hovering over.
        const auto clamped_row_as_f = fabs(ImgBBox.top - MousePosWorld.y )/ImgBBox.height;
        const auto clamped_col_as_f = fabs(MousePosWorld.x - ImgBBox.left)/ImgBBox.width;
        out.clamped_image_pos.x = clamped_row_as_f;
        out.clamped_image_pos.y = clamped_col_as_f;
        out.clamped_image_pos_valid = true;

        const auto img_w_h = disp_img_texture_sprite.first.getSize();
        const auto col_as_u = static_cast<uint32_t>(clamped_col_as_f * static_cast<float>(img_w_h.x));
        const auto row_as_u = static_cast<uint32_t>(clamped_row_as_f * static_cast<float>(img_w_h.y));
        out.pixel_image_pos_row = row_as_u;
        out.pixel_image_pos_col = col_as_u;
        out.pixel_image_pos_valid = true;

        // Get the DICOM coordinates at the centre of the voxel.
        try{
            const auto pix_pos = disp_img_it->position(row_as_u,col_as_u);
            out.mouse_DICOM_pos = pix_pos;
            out.mouse_DICOM_pos_valid = true;
        }catch(const std::exception &){
            return out;
        }

        // Get the DICOM coordinates at the tip of the mouse.
        const auto img_dicom_width = disp_img_it->pxl_dx * disp_img_it->rows;
        const auto img_dicom_height = disp_img_it->pxl_dy * disp_img_it->columns; 
        const auto img_top_left = disp_img_it->anchor + disp_img_it->offset
                                - disp_img_it->row_unit * disp_img_it->pxl_dx * 0.5f
                                - disp_img_it->col_unit * disp_img_it->pxl_dy * 0.5f;
        //const auto img_top_right = img_top_left + disp_img_it->row_unit * img_dicom_width;
        //const auto img_bottom_left = img_top_left + disp_img_it->col_unit * img_dicom_height;

        const auto dicom_pos = img_top_left 
                             + disp_img_it->row_unit * img_dicom_width  * clamped_row_as_f
                             + disp_img_it->col_unit * img_dicom_height * clamped_col_as_f;
        out.voxel_DICOM_pos = dicom_pos;
        out.voxel_DICOM_pos_valid = true;

        return out;
    };

    // Routine for printing current mouse pixel and DICOM coordinates.
    // Also samples the voxel value underneath the mouse, if applicable.
    const auto Update_Mouse_Coords_Voxel_Sample = [&]() -> void {
        auto mc = Convert_Mouse_Coords();

        // Position the text elements.
        if(mc.window_pos_valid){
            cursortext.setPosition(mc.window_pos_row, mc.window_pos_col);
            smallcirc.setPosition(mc.window_pos_row - smallcirc.getRadius(),
                                  mc.window_pos_col - smallcirc.getRadius());
        }

        cursortext.setString("");
        BLcornertextss.str(""); //Clear stringstream.

        //Display pixel intensity and DICOM position at the mouse for the image being hovered over.
        if(mc.mouse_DICOM_pos_valid){
            const auto pix_val = disp_img_it->value(mc.pixel_image_pos_row, mc.pixel_image_pos_col, 0);
            std::stringstream ss;
            ss << "(r,c)=(" << mc.pixel_image_pos_row << "," << mc.pixel_image_pos_col << ") -- " << pix_val;
            ss << "    (DICOM Position)=" << mc.mouse_DICOM_pos;
            cursortext.setString(ss.str());
            BLcornertextss << ss.str();
        }
        return;
    };

    // --------------------------------------------------------------------------------------
    // Event handlers.

    //Show a simple help dialog with some keyboard commands.
    const auto show_help = [&](){
            // Easy way to get list of commands:
            // `grep -C 3 'thechar == ' src/PETCT_Perfusion_Analysis.cc | grep '//\|thechar'`
            Execute_Command_In_Pipe(
                    "zenity --info --no-wrap --text=\""
                    "DICOMautomaton Image Viewer\\n\\n"
                    "\\t Commands: \\n"
                    "\\t\\t h,H \\t Display this help.\\n"
                    "\\t\\t x \\t\\t Toggle whether existing contours should be displayed.\\n"
                    "\\t\\t m \\t\\t Place or remove an invisible marker at the current mouse position for distance measurement.\\n"
                    "\\t\\t d \\t\\t Dump the window contents as an image after the next render.\\n"
                    "\\t\\t D \\t\\t Dump raw pixels for all spatially overlapping images from the current array (e.g., time courses).\\n"
                    "\\t\\t i \\t\\t Dump the current image to file.\\n"
                    "\\t\\t I \\t\\t Dump all images in the current array to file.\\n"
                    "\\t\\t r,c \\t\\t Plot pixel intensity profiles along the mouse\\'s current row and column with Gnuplot.\\n"
                    "\\t\\t R,C \\t\\t Plot realtime pixel intensity profiles along the mouse\\'s current row and column.\\n"
                    "\\t\\t t \\t\\t Plot a time course at the mouse\\'s current row and column.\\n"
                    "\\t\\t T \\t\\t Open a realtime plotting window.\\n"
                    "\\t\\t a,A \\t\\t Plot or dump the pixel values for [a]ll image sets which spatially overlap.\\n"
#ifdef DCMA_USE_GNU_GSL
                    "\\t\\t M \\t\\t Try plot a pharmacokinetic [M]odel using image map parameters and ROI time courses.\\n"
#endif
                    "\\t\\t N,P \\t\\t Advance to the next/previous image series.\\n"
                    "\\t\\t n,p \\t\\t Advance to the next/previous image in this series.\\n"
                    "\\t\\t -,+ \\t\\t Advance to the next/previous image that spatially overlaps this image.\\n"
                    "\\t\\t (,) \\t\\t Cycle through the available colour maps/transformations.\\n"
                    "\\t\\t l,L \\t\\t Reset the image scale to be pixel-for-pixel what is seen on screen.\\n"
                    "\\t\\t u \\t\\t Toggle showing metadata tags that are identical to the neighbouring image\\'s metadata tags.\\n"
                    "\\t\\t U \\t\\t Dump and show the current image\\'s metadata.\\n"
                    "\\t\\t e \\t\\t Erase latest non-empty contour. (A single contour.)\\n"
                    "\\t\\t E \\t\\t Empty the current working ROI buffer. (The entire buffer; all contours.)\\n"
                    "\\t\\t s,S \\t\\t Save the current contour collection.\\n"
                    "\\t\\t # \\t\\t Compute stats for the working, unsaved contour collection.\\n"
                    "\\t\\t % \\t\\t Open a dialog box to select an explicit window and level.\\n"
                    "\\t\\t b \\t\\t Serialize Drover instance (all data) to file.\\n"
                    "\\n\""
            );
            return;
    };

    //Dump a serialization of the current (*entire*) Drover class.
    const auto dump_serialized_drover = [&](){
            const boost::filesystem::path out_fname("/tmp/boost_serialized_drover.xml.gz");
            const bool res = Common_Boost_Serialize_Drover(DICOM_data, out_fname);
            if(res){
                FUNCINFO("Dumped serialization to file " << out_fname.string());
            }else{
                FUNCWARN("Unable dump serialization to file " << out_fname.string());
            }
            return;
    };

    //Cycle through the available colour maps/transformations.
    const auto cycle_colour_maps_next = [&](){
            colour_map = (colour_map + 1) % colour_maps.size();

            if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);
                FUNCINFO("Reloaded texture using '" << colour_maps[colour_map].first << "' colour map");
            }else{
                FUNCERR("Unable to reload texture using selected colour map");
            }
            return;
    };

    const auto cycle_colour_maps_prev = [&](){
            colour_map = (colour_map + colour_maps.size() - 1) % colour_maps.size();

            if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);
                FUNCINFO("Reloaded texture using '" << colour_maps[colour_map].first << "' colour map");
            }else{
                FUNCERR("Unable to reload texture using selected colour map");
            }
            return;
    };

    //Toggle whether existing contours should be displayed.
    const auto toggle_showing_existing_contours = [&](){
            ShowExistingContours = !ShowExistingContours;
            return;
    };

    //Place or remove an invisible marker for measurement in the DICOM coordinate system.
    const auto toggle_measurement_mode = [&](){
            // If a valid point exists, clear it.
            if(tagged_pos){
                tagged_pos = {}; // Reset the optional.

            // If a valid point does not yet exist, try to tag the current mouse point.
            }else{
                auto mc = Convert_Mouse_Coords();
                if(mc.mouse_DICOM_pos_valid){
                    const auto mouse_pos = mc.mouse_DICOM_pos;
                    tagged_pos = mouse_pos;
                }else{
                    FUNCWARN("Unable to place marker: mouse not hovering over an image");
                }
            }
            return;
    };

    //Set the flag for dumping the window contents as an image after the next render.
    const auto initiate_screenshot = [&](){
            DumpScreenshot = true;
            return;
    };

    //Dump raw pixels for all spatially overlapping images from the current array.
    // (Useful for dumping time courses.)
    const auto dump_raw_pixels_for_overlapping_images = [&](){
            //Get a list of images which spatially overlap this point. Order should be maintained.
            const auto pix_pos = disp_img_it->position(0,0);
            const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
            const std::list<vec3<double>> points = { pix_pos, pix_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                              pix_pos - ortho * disp_img_it->pxl_dz * 0.25 };
            auto encompassing_images = (*img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);

            long int count = 0;
            for(auto & pimg : encompassing_images){
                const auto pixel_dump_filename_out = Get_Unique_Sequential_Filename("/tmp/spatially_overlapping_dump_",6,".fits");
                if(WriteToFITS(*pimg,pixel_dump_filename_out)){
                    FUNCINFO("Dumped pixel data for image " << count << " to file '" << pixel_dump_filename_out << "'");
                }else{
                    FUNCWARN("Unable to dump pixel data for image " << count << " to file '" << pixel_dump_filename_out << "'");
                }

            }
            return;
    };

    //Dump the current image to file.
    const auto dump_current_image_to_file = [&](){
            const auto pixel_dump_filename_out = Get_Unique_Sequential_Filename("/tmp/display_image_dump_",6,".fits");
            if(WriteToFITS(*disp_img_it,pixel_dump_filename_out)){
                FUNCINFO("Dumped pixel data for this image to file '" << pixel_dump_filename_out << "'");
            }else{
                FUNCWARN("Unable to dump pixel data for this image to file '" << pixel_dump_filename_out << "'");
            }
            return;
    };

    //Dump all images in the current array to file.
    const auto dump_current_image_array_to_files = [&](){
            long int count = 0;
            for(auto &pimg : (*img_array_ptr_it)->imagecoll.images){
                const auto pixel_dump_filename_out = Get_Unique_Sequential_Filename("/tmp/image_dump_",6,".fits");
                if(WriteToFITS(pimg,pixel_dump_filename_out)){
                    FUNCINFO("Dumped pixel data for image " << count << " to file '" << pixel_dump_filename_out << "'");
                }else{
                    FUNCWARN("Unable to dump pixel data for this image to file '" << pixel_dump_filename_out << "'");
                }
                ++count;
            }
            return;
    };

    //Given the current mouse coordinates, dump pixel intensity profiles along the current row and column.
    const auto dump_rc_aligned_image_intensity_profiles = [&](char r_or_c){
            auto mc = Convert_Mouse_Coords();
            if(!mc.pixel_image_pos_valid){
                FUNCWARN("The mouse is not currently hovering over the image. Cannot dump row/column profiles");
                return;
            }
            const auto row_as_u = mc.pixel_image_pos_row;
            const auto col_as_u = mc.pixel_image_pos_col;

            FUNCINFO("Dumping row and column profiles for row,col = " << row_as_u << "," << col_as_u);

            samples_1D<double> row_profile, col_profile;
            std::stringstream title;

            for(auto i = 0; i < disp_img_it->columns; ++i){
                const auto val_raw = disp_img_it->value(row_as_u,i,0);
                const auto col_num = static_cast<double>(i);
                if(std::isfinite(val_raw)) row_profile.push_back({ col_num, 0.0, val_raw, 0.0 });
            }
            for(auto i = 0; i < disp_img_it->rows; ++i){
                const auto val_raw = disp_img_it->value(i,col_as_u,0);
                const auto row_num = static_cast<double>(i);
                if(std::isfinite(val_raw)) col_profile.push_back({ row_num, 0.0, val_raw, 0.0 });
            }

            try{
                if(r_or_c == 'r'){
                    if(row_profile.size() < 2) throw std::runtime_error("Insufficient data for plot");
                    title << "Profile for row " << row_as_u << ")";

                    YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> row_shtl(row_profile, "Row Profile");
                    YgorMathPlottingGnuplot::Plot<double>({row_shtl}, title.str(), "Pixel Index (row #)", "Pixel Intensity");
                }else{
                    if(col_profile.size() < 2) throw std::runtime_error("Insufficient data for plot");
                    title << "Profile for column " << col_as_u << ")";

                    YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> col_shtl(col_profile, "Col Profile");
                    YgorMathPlottingGnuplot::Plot<double>({col_shtl}, title.str(), "Pixel Index (column #)", "Pixel Intensity");
                }
            }catch(const std::exception &e){
                FUNCWARN("Failed to plot: " << e.what());
            }
            return;
    };

    //Launch/open a realtime plotting window.
    const auto launch_time_plot_window = [&](){
            plotwindow.create(sf::VideoMode(640, 480), "DICOMautomaton Time Courses");
            plotwindow.setFramerateLimit(30);
            plotwindowtype = SecondaryPlot::TimeCourse;
            return;
    };

    const auto launch_row_plot_window = [&](){
            plotwindow.create(sf::VideoMode(640, 480), "DICOMautomaton Row Profile Inspector");
            plotwindow.setFramerateLimit(30);
            plotwindowtype = SecondaryPlot::RowProfile;
            return;
    };

    const auto launch_column_plot_window = [&](){
            plotwindow.create(sf::VideoMode(640, 480), "DICOMautomaton Column Profile Inspector");
            plotwindow.setFramerateLimit(30);
            plotwindowtype = SecondaryPlot::ColumnProfile;
            return;
    };

    //Given the current mouse coordinates, dump a time series at the image pixel over all available images
    // which spatially overlap.
    //
    // So this routine dumps a time course at the mouse pixel.
    //
    const auto dump_voxel_time_series = [&](){
            auto mc = Convert_Mouse_Coords();
            if(!mc.voxel_DICOM_pos_valid){
                FUNCWARN("The mouse is not currently hovering over the image. Cannot dump time course");
                return;
            }
            const auto row_as_u = mc.pixel_image_pos_row;
            const auto col_as_u = mc.pixel_image_pos_col;
            const auto pix_pos = mc.voxel_DICOM_pos;
            FUNCINFO("Dumping time course for row,col = " << row_as_u << "," << col_as_u);

            //Get a list of images which spatially overlap this point. Order should be maintained.
            const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
            const std::list<vec3<double>> points = { pix_pos, pix_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                              pix_pos - ortho * disp_img_it->pxl_dz * 0.25 };
            auto encompassing_images = (*img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);

            //Cycle over the images, dumping the ordinate (pixel values) vs abscissa (time) derived from metadata.
            samples_1D<double> shtl;
            const std::string quantity("dt"); //As it appears in the metadata. Must convert to a double!

            const double radius = 2.1; //Circle of certain radius (in DICOM-coord. system).
            std::stringstream title;
            title << "P_{row,col,rad} = P_{" << row_as_u << "," << col_as_u << "," << radius << "}";
            title << " vs " << quantity << ". ";

            for(const auto &enc_img_it : encompassing_images){
                if(auto abscissa = enc_img_it->GetMetadataValueAs<double>(quantity)){
                    //Circle of certain radius (in DICOM-coord. system).
                    std::list<double> vals;
                    for(auto lrow = 0; lrow < enc_img_it->rows; ++lrow){
                        for(auto lcol = 0; lcol < enc_img_it->columns; ++lcol){
                            const auto row_col_pix_pos = enc_img_it->position(lrow,lcol);
                            if(pix_pos.distance(row_col_pix_pos) <= radius){
                                const auto pix_val = enc_img_it->value(lrow,lcol,0);
                                if(std::isfinite(pix_val)) vals.push_back( static_cast<double>(pix_val) );
                            }
                        }
                    }
                    const auto dabscissa = 0.0;
                    const auto ordinate  = Stats::Mean(vals);
                    const auto dordinate = (vals.size() > 2) ? std::sqrt(Stats::Unbiased_Var_Est(vals))/std::sqrt(1.0*vals.size()) 
                                                             : 0.0;
                    shtl.push_back(abscissa.value(),dabscissa,ordinate,dordinate);
                }
            }


            title << "Time Course. Images encompass " << pix_pos << ". ";
            try{
                YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> ymp_shtl(shtl, "Buffer A");
                YgorMathPlottingGnuplot::Plot<double>({ymp_shtl}, title.str(), "Time (s)", "Pixel Intensity");
            }catch(const std::exception &e){
                FUNCWARN("Failed to plot: " << e.what());
            }
            shtl.Write_To_File(Get_Unique_Sequential_Filename("/tmp/pixel_intensity_time_course_",6,".txt"));
            return;
    };

#ifdef DCMA_USE_GNU_GSL
    //Given the current mouse coordinates, try to show a perfusion model using model parameters from
    // other images. Also show a time course of the raw data for comparison with the model fit.
    const auto show_perfusion_model = [&](){
            auto mc = Convert_Mouse_Coords();
            if(!mc.voxel_DICOM_pos_valid){
                FUNCWARN("The mouse is not currently hovering over the image. Cannot compute perfusion model");
                return;
            }
            const auto row_as_u = mc.pixel_image_pos_row;
            const auto col_as_u = mc.pixel_image_pos_col;
            const auto pix_pos = mc.voxel_DICOM_pos;

            const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
            const std::list<vec3<double>> points = { pix_pos, pix_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                              pix_pos - ortho * disp_img_it->pxl_dz * 0.25 };

            //Metadata quantities of interest.
            const auto  k1A_regex = Compile_Regex(".*k1A.*");
            const auto tauA_regex = Compile_Regex(".*tauA.*");
            const auto  k1V_regex = Compile_Regex(".*k1V.*");
            const auto tauV_regex = Compile_Regex(".*tauV.*");
            const auto   k2_regex = Compile_Regex(".*k2.*");

            enum {
                Have_No_Model,
                Have_1Compartment2Input_5Param_LinearInterp_Model,
                Have_1Compartment2Input_5Param_Chebyshev_Model,
                Have_1Compartment2Input_Reduced3Param_Chebyshev_Model
            } HaveModel;
            HaveModel = Have_No_Model;

            KineticModel_1Compartment2Input_5Param_LinearInterp_Parameters model_5params_linear;
            KineticModel_1Compartment2Input_5Param_Chebyshev_Parameters model_5params_cheby;
            KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Parameters model_3params_cheby;

            try{
                //First pass: look for serialized model_params. Deserialize.
                //
                // Note: we have to do this first, before loading voxel-specific data (e.g., k1A) because the
                // model_state is stripped of individual-voxel-specific data. Deserialization will overwrite 
                // individual-voxel-specific data with NaNs.
                //
                for(auto l_img_array_ptr_it = img_array_ptr_beg; l_img_array_ptr_it != img_array_ptr_end; ++l_img_array_ptr_it){
                    auto encompassing_images = (*l_img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);

                    for(const auto &enc_img_it : encompassing_images){
                        if(auto m_str = enc_img_it->GetMetadataValueAs<std::string>("ModelState")){
                            if(HaveModel == Have_No_Model){
                                if(Deserialize(m_str.value(),model_5params_linear)){
                                    HaveModel = Have_1Compartment2Input_5Param_LinearInterp_Model;
                                }else if(Deserialize(m_str.value(),model_3params_cheby)){
                                    HaveModel = Have_1Compartment2Input_Reduced3Param_Chebyshev_Model;
                                }else if(Deserialize(m_str.value(),model_5params_cheby)){
                                    HaveModel = Have_1Compartment2Input_5Param_Chebyshev_Model;
                                }else{
                                    throw std::runtime_error("Unable to deserialize model parameters.");
                                }
                                return;
                            }
                        }
                    }
                }
                if(HaveModel == Have_No_Model){
                    throw std::logic_error("We should have a valid model here, but do not.");
                }

                //Second pass: locate individual-voxel-specific data needed to evaluate the model.
                std::map<std::string, samples_1D<double>> time_courses;
                for(auto l_img_array_ptr_it = img_array_ptr_beg; l_img_array_ptr_it != img_array_ptr_end; ++l_img_array_ptr_it){
                    auto encompassing_images = (*l_img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);

                    for(const auto &enc_img_it : encompassing_images){
                        //Find the pixel of interest.
                        for(auto l_chnl = 0; l_chnl < enc_img_it->channels; ++l_chnl){
                            double pix_val;
                            try{
                                const auto indx = enc_img_it->index(pix_pos, l_chnl);
                                if(indx < 0) continue;
                            
                                auto rcc = enc_img_it->row_column_channel_from_index(indx);
                                const long int l_row = std::get<0>(rcc);
                                const long int l_col = std::get<1>(rcc);
                                if(l_chnl != std::get<2>(rcc)) continue;

                                pix_val = enc_img_it->value(l_row, l_col, l_chnl);

                            }catch(const std::exception &){
                                continue;
                            }

                            //Now have pixel value. Figure out what to do with it.
                            if(auto desc = enc_img_it->GetMetadataValueAs<std::string>("Description")){

                                if(false){
                                }else if(std::regex_match(desc.value(), k1A_regex)){
                                    model_5params_linear.k1A = pix_val;
                                    model_5params_cheby.k1A  = pix_val;
                                    model_3params_cheby.k1A  = pix_val;
                                }else if(std::regex_match(desc.value(), tauA_regex)){
                                    model_5params_linear.tauA = pix_val;
                                    model_5params_cheby.tauA  = pix_val;
                                    model_3params_cheby.tauA  = pix_val;
                                }else if(std::regex_match(desc.value(), k1V_regex)){
                                    model_5params_linear.k1V = pix_val;
                                    model_5params_cheby.k1V  = pix_val;
                                    model_3params_cheby.k1V  = pix_val;
                                }else if(std::regex_match(desc.value(), tauV_regex)){
                                    model_5params_linear.tauV = pix_val;
                                    model_5params_cheby.tauV  = pix_val;
                                    model_3params_cheby.tauV  = pix_val;
                                }else if(std::regex_match(desc.value(), k2_regex)){
                                    model_5params_linear.k2 = pix_val;
                                    model_5params_cheby.k2  = pix_val;
                                    model_3params_cheby.k2  = pix_val;

                                //Otherwise, if there is a timestamp then assume it is a time course we should show.
                                }else{
                                    if(auto dt = enc_img_it->GetMetadataValueAs<double>("dt")){
                                        time_courses[desc.value()].push_back(dt.value(), 0.0, pix_val, 0.0);
                                    }
                                }
                            }
                            
                        }
                    }
                }

                //Now plot the time courses, evaluate the model, and plot the model.
                {
                    const long int samples = 200;
                    double tmin = std::numeric_limits<double>::infinity();
                    double tmax = -tmin;
                    for(const auto &p : time_courses){
                        const auto extrema = p.second.Get_Extreme_Datum_x();
                        tmin = std::min(tmin, extrema.first[0] - 5.0);
                        tmax = std::max(tmax, extrema.second[0] + 5.0);
                    }
                    if( !std::isfinite(tmin) || !std::isfinite(tmax) ){
                        tmin = -5.0;
                        tmax = 200.0;
                    }
                    const double dt = (tmax - tmin) / static_cast<double>(samples);

                    samples_1D<double> fitted_model;
                    for(long int i = 0; i < samples; ++i){
                        const double t = tmin + dt * i;
                        if(HaveModel == Have_1Compartment2Input_5Param_LinearInterp_Model){
                            KineticModel_1Compartment2Input_5Param_LinearInterp_Results eval_res;
                            Evaluate_Model(model_5params_linear,t,eval_res);
                            fitted_model.push_back(t, 0.0, eval_res.I, 0.0);
                        }else if(HaveModel == Have_1Compartment2Input_5Param_Chebyshev_Model){
                            KineticModel_1Compartment2Input_5Param_Chebyshev_Results eval_res;
                            Evaluate_Model(model_5params_cheby,t,eval_res);
                            fitted_model.push_back(t, 0.0, eval_res.I, 0.0);
                        }else if(HaveModel == Have_1Compartment2Input_Reduced3Param_Chebyshev_Model){
                            KineticModel_1Compartment2Input_Reduced3Param_Chebyshev_Results eval_res;
                            Evaluate_Model(model_3params_cheby,t,eval_res);
                            fitted_model.push_back(t, 0.0, eval_res.I, 0.0);
                        }
                    }

                    std::string model_title = "Fitted model"_s;
                    if(HaveModel == Have_1Compartment2Input_5Param_LinearInterp_Model){
                        model_title += "(1C2I, 5Param, LinearInterp)";
                    }else if(HaveModel == Have_1Compartment2Input_5Param_Chebyshev_Model){
                        model_title += "(1C2I, 5Param, Chebyshev)";
                    }else if(HaveModel == Have_1Compartment2Input_Reduced3Param_Chebyshev_Model){
                        model_title += "(1C2I, Reduced3Param, Chebyshev)";
                    }
                    time_courses[model_title] = fitted_model;
                }

                const std::string Title = "Time course: row = " + std::to_string(row_as_u) + ", col = " + std::to_string(col_as_u);
                PlotTimeCourses(Title, time_courses, {});
            }catch(const std::exception &e){
                FUNCWARN("Unable to reconstruct model: " << e.what());
            }

            return;
    };
#endif // DCMA_USE_GNU_GSL

    //Given the current mouse coordinates, dump the pixel value for [A]ll image sets which spatially overlap.
    // This routine is useful for debugging problematic pixels, or trying to follow per-pixel calculations.
    //
    // NOTE: This routine finds the pixel nearest to the specified voxel in DICOM space. So if the image was
    //       resampled, you will still be able to track the pixel nearest to the centre. In case only exact
    //       pixels should be tracked, the row and column numbers are spit out; so just filter out pixel
    //       coordinates you don't want.
    //
    const auto dump_overlapping_voxels = [&](){
            auto mc = Convert_Mouse_Coords();
            if(!mc.voxel_DICOM_pos_valid){
                FUNCWARN("The mouse is not currently hovering over the image. Cannot dump overlapping pixel values");
                return;
            }
            const auto row_as_u = mc.pixel_image_pos_row;
            const auto col_as_u = mc.pixel_image_pos_col;
            const auto pix_pos = mc.voxel_DICOM_pos;

            const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
            const std::list<vec3<double>> points = { pix_pos, pix_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                              pix_pos - ortho * disp_img_it->pxl_dz * 0.25 };

            const auto FOname = Get_Unique_Sequential_Filename("/tmp/pixel_intensity_from_all_overlapping_images_", 6, ".csv");
            std::fstream FO(FOname, std::fstream::out);
            if(!FO) FUNCERR("Unable to write to the file '" << FOname << "'. Cannot continue");

            //Metadata quantities to also harvest.
            std::vector<std::string> quantities_d = { "dt", "FlipAngle" };
            std::vector<std::string> quantities_s = { "Description" };

            FO << "# Image Array Number, Row, Column, Channel, Pixel Value, ";
            for(auto quantity : quantities_d) FO << quantity << ", ";
            for(auto quantity : quantities_s) FO << quantity << ", ";
            FO << std::endl;

            //Get a list of images which spatially overlap this point. Order should be maintained.
            size_t ImageArrayNumber = 0;
            for(auto l_img_array_ptr_it = img_array_ptr_beg; l_img_array_ptr_it != img_array_ptr_end; ++l_img_array_ptr_it, ++ImageArrayNumber){
                auto encompassing_images = (*l_img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);
                for(const auto &enc_img_it : encompassing_images){
                    //Find the pixel of interest.
                    for(auto l_chnl = 0; l_chnl < enc_img_it->channels; ++l_chnl){
                        long int l_row, l_col;
                        double pix_val;
                        try{
                            const auto indx = enc_img_it->index(pix_pos, l_chnl);
                            if(indx < 0) continue;
                        
                            auto rcc = enc_img_it->row_column_channel_from_index(indx);
                            l_row = std::get<0>(rcc);
                            l_col = std::get<1>(rcc);
                            if(l_chnl != std::get<2>(rcc)) continue;

                            pix_val = enc_img_it->value(l_row, l_col, l_chnl);

                        }catch(const std::exception &e){
                            continue;
                        }
                        FO << ImageArrayNumber << ", ";
                        FO << l_row << ", " << l_col << ", " << l_chnl << ", ";
                        FO << pix_val << ", ";

                        for(auto quantity : quantities_d){
                            if(auto q = enc_img_it->GetMetadataValueAs<double>(quantity)){
                                FO << q.value() << ", ";
                            }
                        }
                        for(auto quantity : quantities_s){
                            if(auto q = enc_img_it->GetMetadataValueAs<std::string>(quantity)){
                                FO << Quote_Static_for_Bash(q.value()) << ", ";
                            }
                        }
                        FO << std::endl;

                    }
                }
            }
            FO.close();
            FUNCINFO("Dumped pixel values which coincide with the specified voxel at"
                     " row,col = " << row_as_u << "," << col_as_u);

            return;
    };

    //Advance to the next/previous Image_Array. Also reset necessary display image iterators.
    //
    // Note: 'N' for next, 'P' for previous.
    const auto advance_to_next_prev_image_array = [&](const long int n){
            //Save the current image position. We will attempt to find the same spot after switching arrays.
            const auto disp_img_pos = static_cast<size_t>( std::distance(disp_img_beg, disp_img_it) );

            custom_width  = std::optional<double>();
            custom_centre = std::optional<double>();

            if(n == 0) return;
            const auto n_clamped = (n > 0) ? 1 : -1;

            if(false){
            }else if(n_clamped == 1){
                if(img_array_ptr_it == img_array_ptr_last){ //Wrap around forwards.
                    img_array_ptr_it = img_array_ptr_beg;
                }else{
                    std::advance(img_array_ptr_it, 1);
                }

            }else if(n_clamped == -1){
                if(img_array_ptr_it == img_array_ptr_beg){ //Wrap around backwards.
                    img_array_ptr_it = img_array_ptr_last;
                }else{
                    std::advance(img_array_ptr_it,-1);
                }

            }else{
                throw std::logic_error("Advancement direction not understood. Cannot continue.");
            }

            FUNCINFO("There are " << (*img_array_ptr_it)->imagecoll.images.size() << " images in this Image_Array");

            disp_img_beg  = (*img_array_ptr_it)->imagecoll.images.begin();
            disp_img_last = std::prev((*img_array_ptr_it)->imagecoll.images.end());
            disp_img_it   = disp_img_beg;

            if(disp_img_pos < (*img_array_ptr_it)->imagecoll.images.size()){
                std::advance(disp_img_it, disp_img_pos);
            }

            if(!contour_coll_shtl.contours.back().points.empty()){
                contour_coll_shtl.contours.emplace_back();
                contour_coll_shtl.contours.back().closed = true;
            }

            if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);
                FUNCINFO("Loaded Image_Array " << std::distance(img_array_ptr_beg,img_array_ptr_it) << ". "
                         "There are " << (*img_array_ptr_it)->imagecoll.images.size() << " images in this Image_Array");
                
            }else{ 
                FUNCERR("Unable to load image --> texture --> sprite");
            }   

            if(auto ImageDesc = disp_img_it->GetMetadataValueAs<std::string>("Description")){
                window.setTitle("DICOMautomaton IV: '"_s + ImageDesc.value() + "'");
            }else{
                window.setTitle("DICOMautomaton IV: <no description available>");
            }

            Update_Mouse_Coords_Voxel_Sample();
            return;
    };

    //Advance to the next/previous display image in the current Image_Array.
    //
    // Note: 'n' can be positive or negative.
    const auto advance_to_next_prev_image = [&](const long int n){
            if(n == 0) return;
            const auto n_clamped = (n > 0) ? 1 : -1;

            if(false){
            }else if(n_clamped == 1){
                if(disp_img_it == disp_img_last){
                    disp_img_it = disp_img_beg;
                }else{
                    std::advance(disp_img_it, 1);
                }

            }else if(n_clamped == -1){
                if(disp_img_it == disp_img_beg){
                    disp_img_it = disp_img_last;
                }else{
                    std::advance(disp_img_it,-1);
                }
            
            }else{
                throw std::logic_error("Advancement direction not understood. Cannot continue.");
            }

            if(!contour_coll_shtl.contours.back().points.empty()){
                contour_coll_shtl.contours.emplace_back();       
                contour_coll_shtl.contours.back().closed = true;
            }

            if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

                //const auto img_number = std::distance((*img_array_ptr_it)->imagecoll.images.begin(), disp_img_it);
                const auto img_number = std::distance(disp_img_beg, disp_img_it);
                FUNCINFO("Loaded next texture in unaltered Image_Array order. Displaying image number " << img_number);
                
            }else{
                FUNCERR("Unable to load image --> texture --> sprite");
            }

            if(auto ImageDesc = disp_img_it->GetMetadataValueAs<std::string>("Description")){
                window.setTitle("DICOMautomaton IV: '"_s + ImageDesc.value() + "'");
            }else{
                window.setTitle("DICOMautomaton IV: <no description available>");
            }

            //Scale the image to fill the available space.
            scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

            Update_Mouse_Coords_Voxel_Sample();
            return;
    };

    //Sample pixels from an external image into the current frame.
    const auto overwrite_current_frame = [&](){
            do{ // Does not loop, just lets us break out.
                //std::string fname;
                //std::cout << std::endl;
                //std::cout << "Please enter the filename you wish to sample from: " << std::endl;
                //std::cin >> fname;
                const auto raw_fname = Execute_Command_In_Pipe(
                            "zenity --title='Select a file to sample from (FITS format).' --file-selection --separator='\n' 2>/dev/null");
                const auto fname_vec = SplitStringToVector(raw_fname + "\n\n", '\n', 'd');
                if(fname_vec.empty()) break;
                const std::string fname = fname_vec.front();

                planar_image<float,double> casted_img; // External image with casted pixel values.
                bool loaded = false;

                if(!loaded){
                    try{
                        auto animg = ReadFromFITS<uint8_t,double>(fname);
                        casted_img.cast_from(animg);
                        loaded = true;
                    }catch(const std::exception &e){ };
                }
                if(!loaded){
                    try{
                        auto animg = ReadFromFITS<float,double>(fname);
                        casted_img = animg;
                        loaded = true;
                    }catch(const std::exception &e){ };
                }
                if(!loaded){
                    FUNCINFO("Cannot load file '" << fname << "'");
                    break;
                }

                //Sample the image by ignoring aspect ratio and scaling dimensions to fit.
                const auto r_scale = static_cast<double>(casted_img.rows)    / static_cast<double>(disp_img_it->rows);
                const auto c_scale = static_cast<double>(casted_img.columns) / static_cast<double>(disp_img_it->columns);
                for(long int ch = 0; ch < disp_img_it->channels; ++ch){
                    for(long int r = 0; r < disp_img_it->rows; ++r){
                        for(long int c = 0; c < disp_img_it->columns; ++c){
                            const auto clamped_r = static_cast<double>(r) * r_scale;
                            const auto clamped_c = static_cast<double>(c) * c_scale;
                            const long int clamped_ch = (ch >= casted_img.channels) ? 0 : ch;

                            disp_img_it->reference(r,c,ch) = 
                                casted_img.bilinearly_interpolate_in_pixel_number_space(clamped_r, clamped_c, clamped_ch);
                                //casted_img.bicubically_interpolate_in_pixel_number_space(clamped_r, clamped_c, clamped_ch);
                        }
                    }
                }

                if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                    scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

                    const auto img_number = std::distance(disp_img_beg, disp_img_it);
                    FUNCINFO("Loaded next texture in unaltered Image_Array order. Displaying image number " << img_number);
                    
                }else{
                    FUNCERR("Unable to load image --> texture --> sprite");
                }

                //Scale the image to fill the available space.
                scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

                Update_Mouse_Coords_Voxel_Sample();
            }while(false);
            return;
    };

    //Flood the current image with a uniform pixel intensity.
    const auto flood_current_pixels = [&](){
            float intensity;
            std::cout << std::endl;
            std::cout << "Please enter the intensity to flood with: " << std::endl;
            std::cin >> intensity;

            for(long int ch = 0; ch < disp_img_it->channels; ++ch){
                for(long int r = 0; r < disp_img_it->rows; ++r){
                    for(long int c = 0; c < disp_img_it->columns; ++c){
                        disp_img_it->reference(r,c,ch) = intensity;
                    }
                }
            }

            if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

                const auto img_number = std::distance(disp_img_beg, disp_img_it);
                FUNCINFO("Loaded next texture in unaltered Image_Array order. Displaying image number " << img_number);
                
            }else{
                FUNCERR("Unable to load image --> texture --> sprite");
            }

            //Scale the image to fill the available space.
            scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

            Update_Mouse_Coords_Voxel_Sample();
            return;
    };

    //Step to the next/previous image which spatially overlaps with the current display image.
    const auto advance_to_next_prev_overlapping_image = [&](const long int n){
            const auto disp_img_pos = disp_img_it->center();

            //Get a list of images which spatially overlap this point. Order should be maintained.
            const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
            const std::list<vec3<double>> points = { disp_img_pos, disp_img_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                                   disp_img_pos - ortho * disp_img_it->pxl_dz * 0.25 };
            auto encompassing_images = (*img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);

            //Find the images neighbouring the current image.
            auto enc_img_it = encompassing_images.begin();
            for(  ; enc_img_it != encompassing_images.end(); ++enc_img_it){
                if(*enc_img_it == disp_img_it) break;
            }
            if(enc_img_it == encompassing_images.end()){
                FUNCWARN("Unable to step over spatially overlapping images. None found");
            }else{
                if(n == 0) return;
                const auto n_clamped = (n > 0) ? 1 : -1;

                if(false){
                }else if(n_clamped == 1){
                    ++enc_img_it;
                    if(enc_img_it == encompassing_images.end()){
                        disp_img_it = encompassing_images.front();
                    }else{
                        disp_img_it = *enc_img_it;
                    }

                }else if(n_clamped == -1){
                    if(enc_img_it == encompassing_images.begin()){
                        disp_img_it = encompassing_images.back();
                    }else{
                        --enc_img_it;
                        disp_img_it = *enc_img_it;
                    }
                
                }else{
                    throw std::logic_error("Advancement direction not understood. Cannot continue.");
                }
            }

            if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

                const auto img_number = std::distance(disp_img_beg, disp_img_it);
                FUNCINFO("Loaded next/previous spatially-overlapping texture. Displaying image number " << img_number);

            }else{
                FUNCERR("Unable to load image --> texture --> sprite");
            }

            if(auto ImageDesc = disp_img_it->GetMetadataValueAs<std::string>("Description")){
                window.setTitle("DICOMautomaton IV: '"_s + ImageDesc.value() + "'");
            }else{
                window.setTitle("DICOMautomaton IV: <no description available>");
            }
           
            //Scale the image to fill the available space.
            scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

            Update_Mouse_Coords_Voxel_Sample();
            return;
    };

    //Reset the image scale to be pixel-for-pixel what is seen on screen. (Unless there is a view
    // that has some transformation over on-screen objects.)
    const auto reset_image_scale = [&](){
            disp_img_texture_sprite.second.setScale(1.0f,1.0f);

            Update_Mouse_Coords_Voxel_Sample();
            return;
    };

    //Toggle showing metadata tags that are identical to the neighbouring image's metadata tags.
    const auto toggle_showing_adjacently_different_metadata = [&](){
            OnlyShowTagsDifferentToNeighbours = !OnlyShowTagsDifferentToNeighbours;
            return;
    };

    //Dump to file and show a pop-up window with the full metadata of the present image.
    const auto dump_and_expand_image_metadata = [&](){
            const auto FOname = Get_Unique_Sequential_Filename("/tmp/image_metadata_dump_", 6, ".txt");
            try{
                //Dump the metadata to a file.
                {
                    std::fstream FO(FOname, std::fstream::out);
                    if(!FO) throw std::runtime_error("Unable to write metadata to file.");
                    for(const auto &apair : disp_img_it->metadata){
                        FO << apair.first << " : " << apair.second << std::endl;
                    }
                }

                //Notify that the file has been created.
                FUNCINFO("Dumped metadata to file '" << FOname << "'");

                //Try launch a pop-up window with the metadata from the file displayed.
                std::stringstream ss;
                ss << "zenity --text-info --no-wrap --filename='" << FOname << "' 2>/dev/null";
                Execute_Command_In_Pipe(ss.str());

            }catch(const std::exception &e){ 
                FUNCWARN("Metadata dump failed: " << e.what());
            }

            return;
    };

    //Erase the present contour, or, if empty, the previous non-empty contour. (Not the whole organ.)
    const auto erase_most_recently_drawn_contour = [&](){
            try{
                const std::string erase_roi = Detox_String(Execute_Command_In_Pipe(
                        "zenity --question --text='Erase current or previous non-empty contour?' 2>/dev/null && echo 1"));
                if(erase_roi != "1"){
                    FUNCINFO("Not erasing contours. Here it is for inspection purposes:" << contour_coll_shtl.write_to_string());
                    throw std::runtime_error("Instructed not to erase contour.");
                }

                //Trim empty contours from the shuttle.
                contour_coll_shtl.Purge_Contours_Below_Point_Count_Threshold(1);
                if(contour_coll_shtl.contours.empty()) throw std::runtime_error("Nothing to erase.");

                //Erase the last contour.
                const std::string c_as_str( contour_coll_shtl.contours.back().write_to_string() );                             
                FUNCINFO("About to erase contour. Here it is for inspection purposes: " << c_as_str);
                contour_coll_shtl.contours.pop_back();
                
                //Provide an empty contour for future contouring.
                contour_coll_shtl.contours.emplace_back();
                contour_coll_shtl.contours.back().closed = true;

                FUNCINFO("Latest non-empty contour erased");
            }catch(const std::exception &){ }
            return;
    };

    //Erase or Empty the current working contour buffer. 
    const auto purge_whole_contour_buffer = [&](){
            try{
                const std::string erase_roi = Detox_String(Execute_Command_In_Pipe(
                        "zenity --question --text='Erase whole working ROI?' 2>/dev/null && echo 1"));
                if(erase_roi != "1"){
                    FUNCINFO("Not erasing contours. Here it is for inspection purposes:" << contour_coll_shtl.write_to_string());
                    throw std::runtime_error("Instructed not to clear contour buffer.");
                }

                //Clear the data in preparation for the next contour collection.
                contour_coll_shtl.contours.clear();
                contour_coll_shtl.contours.emplace_back();
                contour_coll_shtl.contours.back().closed = true;

                FUNCINFO("Contour collection cleared from working buffer");
            }catch(const std::exception &){ }
            return;
    };

    //Save the current contour collection.
    const auto save_contour_buffer = [&](){
            try{
                // Ask the user for additional information.
                const std::string save_roi = Detox_String(Execute_Command_In_Pipe("zenity --question --text='Save ROI?' 2>/dev/null && echo 1"));
                if(save_roi != "1"){
                    FUNCINFO("Not saving contours. Here it is for inspection purposes:" << contour_coll_shtl.write_to_string());
                    throw std::runtime_error("Instructed not to save.");
                }

                const std::string roi_name = Detox_String(Execute_Command_In_Pipe(
                    "zenity --entry --text='What is the name of the ROI?' --entry-text='unspecified' 2>/dev/null"));
                if(roi_name.empty()) throw std::runtime_error("Cannot save with an empty ROI name. (Punctuation is removed.)");

                //Trim empty contours from the shuttle.
                contour_coll_shtl.Purge_Contours_Below_Point_Count_Threshold(3);
                if(contour_coll_shtl.contours.empty()) throw std::runtime_error("Given empty contour collection. Contours need >3 points each.");
                const std::string cc_as_str( contour_coll_shtl.write_to_string() );                             

                //Add metadata.
                auto FrameofReferenceUID = disp_img_it->GetMetadataValueAs<std::string>("FrameofReferenceUID");
                if(FrameofReferenceUID){
                    contour_coll_shtl.Insert_Metadata("FrameofReferenceUID", FrameofReferenceUID.value());
                }else{
                    throw std::runtime_error("Missing 'FrameofReferenceUID' metadata element. Cannot continue.");
                }

                auto StudyInstanceUID = disp_img_it->GetMetadataValueAs<std::string>("StudyInstanceUID");
                if(StudyInstanceUID){
                    contour_coll_shtl.Insert_Metadata("StudyInstanceUID", StudyInstanceUID.value());
                }else{
                    throw std::runtime_error("Missing 'StudyInstanceUID' metadata element. Cannot continue.");
                }

                contour_coll_shtl.Insert_Metadata("ROIName", roi_name);
                contour_coll_shtl.Insert_Metadata("NormalizedROIName", X(roi_name));
                contour_coll_shtl.Raw_ROI_name = roi_name;
                contour_coll_shtl.ROI_number = 1000;
                contour_coll_shtl.Minimum_Separation = disp_img_it->pxl_dz;


                //Insert the contours into the Drover object.
                if(DICOM_data.contour_data == nullptr){
                    std::unique_ptr<Contour_Data> output (new Contour_Data());
                    DICOM_data.contour_data = std::move(output);
                }
                DICOM_data.contour_data->ccs.emplace_back(contour_coll_shtl);

                //Clear the data in preparation for the next contour collection.
                contour_coll_shtl.contours.clear();
                contour_coll_shtl.contours.emplace_back();
                contour_coll_shtl.contours.back().closed = true;

                FUNCINFO("Drover class imbued with new contour collection");
            }catch(const std::exception &e){
                FUNCWARN("Unable to save contour collection: '" << e.what() << "'");
            }
            return;
    };

    //Compute some stats for the working, unsaved contour collection.
    const auto compute_stats_for_contour_buffer = [&](){
            try{
                //Trim empty contours from the shuttle.
                auto cccopy = contour_coll_shtl;
                cccopy.Purge_Contours_Below_Point_Count_Threshold(3);
                if(cccopy.contours.empty()) throw std::runtime_error("Given empty contour collection. Contours need >3 points each.");

                //Create a dummy shuttle with the working contours.
                std::list<std::reference_wrapper<contour_collection<double>>> cc_ROI;
                auto base_ptr = reinterpret_cast<contour_collection<double> *>(&contour_coll_shtl);
                base_ptr->Insert_Metadata("ROIName", "working_ROI");
                cc_ROI.push_back( std::ref(*base_ptr) );

                //Accumulate the voxel intensity distributions.
                AccumulatePixelDistributionsUserData ud;
                if(!(*img_array_ptr_it)->imagecoll.Compute_Images( AccumulatePixelDistributions, { },
                                                           cc_ROI, &ud )){
                    throw std::runtime_error("Unable to accumulate pixel distributions.");
                }

                std::stringstream ss;
                for(const auto &av : ud.accumulated_voxels){
                    const auto lROIname = av.first;
                    const auto PixelMean = Stats::Mean( av.second );
                    const auto PixelMedian = Stats::Median( av.second );
                    const auto PixelStdDev = std::sqrt(Stats::Unbiased_Var_Est( av.second ));
        
                    ss << "PixelMedian=" << PixelMedian << ", "
                       << "PixelMean=" << PixelMean << ", "
                       << "PixelStdDev=" << PixelStdDev << ", "
                       << "SNR=" << PixelMean/PixelStdDev << ", "
                       << "VoxelCount=" << av.second.size() << std::endl;
                }
                FUNCINFO("Working contour collection stats:\n\t" << ss.str());

            }catch(const std::exception &e){
                FUNCWARN("Unable to compute working contour collection stats: '" << e.what() << "'");
            }
            return;
    };

    //Query the user to provide a window and level explicitly.
    const auto query_for_window_and_level = [&](){
            try{
                const std::string low_str = Detox_String(Execute_Command_In_Pipe(
                    "zenity --entry --text='What is the new window low?' --entry-text='100.0' 2>/dev/null"));
                const std::string high_str = Detox_String(Execute_Command_In_Pipe(
                    "zenity --entry --text='What is the new window high?' --entry-text='500.0' 2>/dev/null"));

                // Parse the values and protect against mixing low and high values.
                const auto new_low  = std::stod(low_str);
                const auto new_high = std::stod(high_str);
                const auto new_fullwidth = std::abs(new_high - new_low);
                const auto new_centre = std::min(new_low, new_high) + 0.5 * new_fullwidth;
                custom_width.emplace(new_fullwidth);
                custom_centre.emplace(new_centre);

                if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                    scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);
                }else{
                    FUNCERR("Unable to reload image after adjusting window/level");
                }
            }catch(const std::exception &e){
                FUNCWARN("Unable to parse window and level: '" << e.what() << "'");
            }
            return;
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////  Main loop  ///////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////

    //Run until the window is closed or the user wishes to exit.
    while(window.isOpen()){
        BRcornertextss.str(""); //Clear stringstream.

        //Check if any events have accumulated since the last poll. If so, deal with them.
        sf::Event event;
        while(window.pollEvent(event)){
            if(event.type == sf::Event::Closed){
                window.close();
                break;
            }else if(window.hasFocus() && (event.type == sf::Event::KeyPressed)){
                if(event.key.code == sf::Keyboard::Escape){
                    window.close();
                    break;
                }

            }else if(window.hasFocus() && (event.type == sf::Event::KeyReleased)){

            }else if(window.hasFocus() && (event.type == sf::Event::TextEntered) 
                                       && (event.text.unicode < 128)){
                //Not the same as KeyPressed + KeyReleased. Think unicode characters, or control keys.
                const auto thechar = static_cast<char>(event.text.unicode);

                if( false ){

                //Show a simple help dialog with some keyboard commands.
                }else if( (thechar == 'h') || (thechar == 'H') ){
                    show_help();

                //Dump a serialization of the current (*entire*) Drover class.
                }else if( thechar == 'b' ){
                    dump_serialized_drover();

                //Cycle through the available colour maps/transformations.
                }else if( thechar == ')' ){
                    cycle_colour_maps_next();

                }else if( thechar == '(' ){
                    cycle_colour_maps_prev();

                //Toggle whether existing contours should be displayed.
                }else if( thechar == 'x' ){
                    toggle_showing_existing_contours();

                //Place or remove an invisible marker for measurement in the DICOM coordinate system.
                }else if( thechar == 'm' ){
                    toggle_measurement_mode();

                //Set the flag for dumping the window contents as an image after the next render.
                }else if( thechar == 'd' ){
                    initiate_screenshot();

                //Dump raw pixels for all spatially overlapping images from the current array.
                // (Useful for dumping time courses.)
                }else if( thechar == 'D' ){
                    dump_raw_pixels_for_overlapping_images();

                //Dump the current image to file.
                }else if(thechar == 'i'){
                    dump_current_image_to_file();

                //Dump all images in the current array to file.
                }else if(thechar == 'I'){
                    dump_current_image_array_to_files();

                //Given the current mouse coordinates, dump pixel intensity profiles along the current row and column.
                }else if(thechar == 'r'){
                    dump_rc_aligned_image_intensity_profiles('r');
                }else if(thechar == 'c'){
                    dump_rc_aligned_image_intensity_profiles('c');

                //Launch/open a realtime plotting window.
                }else if( thechar == 'T' ){
                    launch_time_plot_window();

                //Launch/open a realtime plotting window.
                }else if( thechar == 'R' ){
                    launch_row_plot_window();

                //Launch/open a realtime plotting window.
                }else if( thechar == 'C' ){
                    launch_column_plot_window();

                //Given the current mouse coordinates, dump a time series at the image pixel over all available images
                // which spatially overlap.
                }else if( thechar == 't' ){
                    dump_voxel_time_series();

#ifdef DCMA_USE_GNU_GSL
                //Given the current mouse coordinates, try to show a perfusion model using model parameters from
                // other images. Also show a time course of the raw data for comparison with the model fit.
                }else if( (thechar == 'M') ){
                    show_perfusion_model();
#endif // DCMA_USE_GNU_GSL

                //Given the current mouse coordinates, dump the pixel value for [A]ll image sets which spatially overlap.
                // This routine is useful for debugging problematic pixels, or trying to follow per-pixel calculations.
                }else if( (thechar == 'a') || (thechar == 'A') ){
                    dump_overlapping_voxels();

                //Advance to the next/previous Image_Array. Also reset necessary display image iterators.
                }else if( thechar == 'N' ){
                    advance_to_next_prev_image_array(1);
                }else if( thechar == 'P' ){
                    advance_to_next_prev_image_array(-1);

                //Advance to the next/previous display image in the current Image_Array.
                }else if( thechar == 'n' ){
                    advance_to_next_prev_image(1);
                }else if( thechar == 'p' ){
                    advance_to_next_prev_image(-1);

                //Sample pixels from an external image into the current frame.
                }else if( thechar == 'f' ){
                    overwrite_current_frame();

                //Flood the current image with a uniform pixel intensity.
                }else if( thechar == 'F' ){
                    flood_current_pixels();

                //Step to the next/previous image which spatially overlaps with the current display image.
                }else if( (thechar == '+') || (thechar == '=') ){
                    advance_to_next_prev_overlapping_image(1);
                }else if( (thechar == '-') || (thechar == '_') ){
                    advance_to_next_prev_overlapping_image(-1);

                //Reset the image scale to be pixel-for-pixel what is seen on screen. (Unless there is a view
                // that has some transformation over on-screen objects.)
                }else if( (thechar == 'l') || (thechar == 'L')){
                    reset_image_scale();

                //Toggle showing metadata tags that are identical to the neighbouring image's metadata tags.
                }else if( thechar == 'u' ){
                    toggle_showing_adjacently_different_metadata();

                //Dump to file and show a pop-up window with the full metadata of the present image.
                }else if( thechar == 'U' ){
                    dump_and_expand_image_metadata();

                //Erase the present contour, or, if empty, the previous non-empty contour. (Not the whole organ.)
                }else if( thechar == 'e' ){
                    erase_most_recently_drawn_contour();

                //Erase or Empty the current working contour buffer. 
                }else if( thechar == 'E' ){
                    purge_whole_contour_buffer();

                //Save the current contour collection.
                }else if( (thechar == 's') || (thechar == 'S') ){
                    save_contour_buffer();

                //Compute some stats for the working, unsaved contour collection.
                }else if( thechar == '#' ){
                    compute_stats_for_contour_buffer();

                //Query the user to provide a window and level explicitly.
                }else if( thechar == '%' ){
                    query_for_window_and_level();

                }else{
                    FUNCINFO("Character '" << thechar << "' is not yet bound to any action");
                }

            }else if(window.hasFocus() && (event.type == sf::Event::MouseWheelMoved)){
                const auto delta = static_cast<double>(event.mouseWheel.delta);
                const auto pressing_shift = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift);
                const auto pressing_control = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl);

                //Ensure there is an existing custom WL. If not, set up some reasonable default.
                if(pressing_shift || pressing_control){
                    const auto ExistingCustomWL = (custom_centre && custom_width);
                    if(!ExistingCustomWL){
                        //Try grab the current image's WL (iff it is valid).
                        auto img_win_valid = disp_img_it->GetMetadataValueAs<std::string>("WindowValidFor");
                        auto img_desc      = disp_img_it->GetMetadataValueAs<std::string>("Description");
                        auto img_win_c     = disp_img_it->GetMetadataValueAs<double>("WindowCenter");
                        auto img_win_fw    = disp_img_it->GetMetadataValueAs<double>("WindowWidth");
                        const auto ImgWLValid = (img_win_valid && img_desc && img_win_c 
                                                 && img_win_fw && (img_win_valid.value() == img_desc.value()));
                        if(ImgWLValid){
                            // Clang complains pre-c++17 about using swap here...
                            //custom_width.swap( img_win_fw );
                            //custom_centre.swap( img_win_c );
                            custom_width = img_win_fw;
                            custom_centre = img_win_c;
                        }else{
                            const auto pixel_minmax_allchnls = disp_img_it->minmax();
                            const auto lowest = std::get<0>(pixel_minmax_allchnls);
                            const auto highest = std::get<1>(pixel_minmax_allchnls);
                            custom_width.emplace(highest-lowest); //Full width.
                            custom_centre.emplace(0.5*(highest+lowest));
                        }
                    }
                }

                //Adjust the custom window and level if a control key is being pressed.
                if(pressing_shift){
                    custom_centre.value() += -delta * 0.10 * custom_width.value();
                }

                if(pressing_control){
                    custom_width.value() *= std::pow(0.95, 0.0-delta);
                }

                //Re-draw the image.
                if(pressing_shift || pressing_control){
                    if(load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
                        scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);
                    }else{
                        FUNCERR("Unable to reload image after adjusting window/level");
                    }
                }

            }else if(window.hasFocus() && (event.type == sf::Event::MouseButtonPressed)){

                if(event.mouseButton.button == sf::Mouse::Left){
                    auto mc = Convert_Mouse_Coords();
                    if(!mc.mouse_DICOM_pos_valid){
                        FUNCWARN("The mouse is not currently hovering over the image. Cannot place contour vertex");
                        break;
                    }
                    const auto row_as_u = mc.pixel_image_pos_row;
                    const auto col_as_u = mc.pixel_image_pos_col;
                    const auto mouse_pos = mc.mouse_DICOM_pos;

                    sf::Uint8 newpixvals[4] = {255, 0, 0, 255};
                    disp_img_texture_sprite.first.update(newpixvals,1,1,col_as_u,row_as_u);
                    disp_img_texture_sprite.second.setTexture(disp_img_texture_sprite.first);

                    auto FrameofReferenceUID = disp_img_it->GetMetadataValueAs<std::string>("FrameofReferenceUID");
                    if(FrameofReferenceUID){
                        //Record the point in the working contour buffer.
                        contour_coll_shtl.contours.back().closed = true;
                        contour_coll_shtl.contours.back().points.push_back( mouse_pos );
                        contour_coll_shtl.contours.back().metadata["FrameofReferenceUID"] = FrameofReferenceUID.value();
                    }else{
                        FUNCWARN("Unable to find display image's FrameofReferenceUID. Cannot insert point in contour");

                    }
                }

            }else if(window.hasFocus() && (event.type == sf::Event::MouseButtonReleased)){

            }else if(window.hasFocus() && (event.type == sf::Event::MouseMoved)){
                Update_Mouse_Coords_Voxel_Sample();

            }else if(event.type == sf::Event::Resized){
                sf::View view;

                //Shrink the image depending on the amount of window space available. The image might disappear off the
                // screen if the window is too small, but nothing gets squished.
                view.reset(sf::FloatRect(0, 0, event.size.width, event.size.height));
                window.setView(view);

                //Scale the image with the window. Results in highly-squished display, and essentially arbitrary world
                // coordinate system.
                scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);

            }else if(window.hasFocus() && (event.type == sf::Event::MouseMoved)){
            }else if(event.type == sf::Event::LostFocus){
            }else if(event.type == sf::Event::GainedFocus){
            }else if(event.type == sf::Event::MouseEntered){
            }else if(event.type == sf::Event::MouseLeft){
            }else{
                FUNCINFO("Ignored event!");
            }

        }

        // ------------------------------ Plotting Window Events ----------------------------------------

        if(plotwindow.isOpen()){
            const auto close_plotwindow = [&]() -> void {
                plotwindow.close();
                plotwindowtype = SecondaryPlot::None;
            };

            sf::Event event;
            while(plotwindow.pollEvent(event)){
                if(event.type == sf::Event::Closed){
                    close_plotwindow();
                    break;
                }else if(window.hasFocus() && (event.type == sf::Event::TextEntered) 
                                           && (event.text.unicode < 128)){
                    const auto thechar = static_cast<char>(event.text.unicode);
                    if(thechar == 'q'){
                        close_plotwindow();
                        break;
                    }else{
                        FUNCINFO("Plotting plotwindow: keypress not yet bound to any action");
                    }
                }else if(plotwindow.hasFocus() && (event.type == sf::Event::KeyPressed)){
                    if(event.key.code == sf::Keyboard::Escape){
                        close_plotwindow();
                        break;
                    }else{
                        FUNCINFO("Plotting plotwindow: keypress not yet bound to any action");
                    }

                }else if(event.type == sf::Event::Resized){
                    sf::View view;
                    view.reset(sf::FloatRect(0, 0, event.size.width, event.size.height));
                    plotwindow.setView(view);
                }else if(event.type == sf::Event::LostFocus){
                }else if(event.type == sf::Event::GainedFocus){
                }else if(event.type == sf::Event::MouseEntered){
                }else if(event.type == sf::Event::MouseLeft){
                }else{
                    FUNCINFO("Ignored event!");
                }
            }
        }


        // -------------------------------------- Rendering ----------------------------------------

        //Populate the corner text with all non-empty info available.
        if(OnlyShowTagsDifferentToNeighbours 
        && ( (*img_array_ptr_it)->imagecoll.images.size() > 1) ){
            //Get the neighbouring image in the present collection.
            auto next_img_it = (disp_img_it == disp_img_last) ? disp_img_beg : std::next(disp_img_it);

            for(const auto &apair : disp_img_it->metadata){
                if(apair.second.empty()) continue;
                const auto matching_pair_it = next_img_it->metadata.find(apair.first);
                if(matching_pair_it == next_img_it->metadata.end()) continue;
                if(apair.second == matching_pair_it->second) continue;
                std::string thekey, theval;
                if(apair.first.size() < 40){
                     thekey = apair.first;
                }else{
                     thekey = apair.first.substr(0,30) + "..." + apair.first.substr(apair.first.size() - 7);
                }
                if(apair.second.size() < 40){
                     theval = apair.second;
                }else{
                     theval = apair.second.substr(0,30) + "..." + apair.second.substr(apair.second.size() - 7);
                }

                BRcornertextss << thekey << " = " << theval << std::endl;
            }

        }else{
            for(const auto &apair : disp_img_it->metadata){
                if(apair.second.empty()) continue;
                std::string thekey, theval;
                if(apair.first.size() < 40){
                     thekey = apair.first;
                }else{
                     thekey = apair.first.substr(0,30) + "..." + apair.first.substr(apair.first.size() - 7);
                }
                if(apair.second.size() < 40){
                     theval = apair.second;
                }else{
                     theval = apair.second.substr(0,30) + "..." + apair.second.substr(apair.second.size() - 7);
                }

                BRcornertextss << thekey << " = " << theval << std::endl;
            }
        }

        BRcornertextss << "offset = " << disp_img_it->offset << std::endl;
        BRcornertextss << "anchor = " << disp_img_it->anchor << std::endl;
        BRcornertextss << "pxl_dx,dy,dz = " << disp_img_it->pxl_dx << ", " 
                                            << disp_img_it->pxl_dy << ", " 
                                            << disp_img_it->pxl_dz << ", " << std::endl;


        //Begin drawing the window contents.
        window.clear(sf::Color::Black);
        //window.draw(ashape);

        window.draw(disp_img_texture_sprite.second);

        BRcornertext.setString(BRcornertextss.str());
        BLcornertext.setString(BLcornertextss.str());
        //Move the text to the proper corner.
        {
            const auto item_bbox = BRcornertext.getGlobalBounds();
            const auto item_brc  = sf::Vector2f( item_bbox.left + item_bbox.width, item_bbox.top + item_bbox.height );

            const auto wndw_view = window.getView();
            const auto view_cntr = wndw_view.getCenter();
            const auto view_size = wndw_view.getSize();
            const auto view_brc  = sf::Vector2f( view_cntr.x + 0.48f*view_size.x, view_cntr.y + 0.48f*view_size.y );
     
            //We should have that the item's bottom right corner coincides with the window's bottom right corner.
            const sf::Vector2f offset = view_brc - item_brc;

            //Now move the text over.
            BRcornertext.move(offset);
        }            
        {
            {
                const auto existing = BLcornertext.getString();
                std::stringstream ss;
                ss << existing.toAnsiString() << std::endl 
                   << "Colour map: " << colour_maps[colour_map].first;
                BLcornertext.setString(ss.str());
            }
            if(custom_centre && custom_width){
                const auto existing = BLcornertext.getString();
                std::stringstream ss;
                ss << existing.toAnsiString() << std::endl 
                   << "Custom c/w: " << custom_centre.value()
                   << " / " << custom_width.value();
                BLcornertext.setString(ss.str());
            }
            if(tagged_pos){
                auto mc = Convert_Mouse_Coords();
                if(mc.mouse_DICOM_pos_valid){
                    const auto mouse_pos = mc.mouse_DICOM_pos;

                    const auto existing = BLcornertext.getString();
                    std::stringstream ss;
                    ss << existing.toAnsiString() << std::endl 
                       << "Distance from " << tagged_pos.value() << ": " << tagged_pos.value().distance(mouse_pos);
                    BLcornertext.setString(ss.str());
                }
            }

            const auto item_bbox = BLcornertext.getGlobalBounds();
            const auto item_blc  = sf::Vector2f( item_bbox.left, item_bbox.top + item_bbox.height );

            const auto wndw_view = window.getView();
            const auto view_cntr = wndw_view.getCenter();
            const auto view_size = wndw_view.getSize();
            const auto view_blc  = sf::Vector2f( view_cntr.x - 0.48f*view_size.x, view_cntr.y + 0.48f*view_size.y );

            //We should have that the item's bottom right corner coincides with the window's bottom right corner.
            const sf::Vector2f offset = view_blc - item_blc;

            //Now move the text over.
            BLcornertext.move(offset);
        }

        window.draw(BRcornertext);
        if(!SingleScreenshot) window.draw(smallcirc); // Draw a circle around the mouse pointer.
        if(drawcursortext) window.draw(cursortext);
        window.draw(BLcornertext);

        //Draw any contours that lie in the plane of the current image. Also draw contour names if the cursor is 'within' them.
        if(ShowExistingContours && (DICOM_data.contour_data != nullptr)){
            sf::Text contourtext;
            std::stringstream contourtextss;
            contourtext.setFont(afont);
            contourtext.setString("");
            contourtext.setCharacterSize(12); //Size in pixels, not in points.
            contourtext.setFillColor(sf::Color::Green);
            contourtext.setOutlineColor(sf::Color::Green);

            for(auto & cc : DICOM_data.contour_data->ccs){
                //auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
                for(auto & c : cc.contours){
                    if(!c.points.empty() 
                    && ( 
                          // Permit contours with any included vertices or at least the 'centre' within the image.
                          ( disp_img_it->sandwiches_point_within_top_bottom_planes(c.Average_Point())
                            || disp_img_it->encompasses_any_of_contour_of_points(c) )
                          || 
                          ( disp_img_it->pxl_dz <= std::numeric_limits<double>::min() ) // //Permit contours on purely 2D images.
                       ) ){
                        sf::VertexArray lines;
                        lines.setPrimitiveType(sf::LinesStrip);

                        //Change colour depending on the orientation.
                        const auto arb_pos_unit = disp_img_it->row_unit.Cross(disp_img_it->col_unit).unit();
                        vec3<double> c_orient;
                        try{ // Protect against degenerate contours. (Should we instead ignore them altogether?)
                            c_orient = c.Estimate_Planar_Normal();
                        }catch(const std::exception &){
                            c_orient = arb_pos_unit;
                        }
                        const auto c_orient_pos = (c_orient.Dot(arb_pos_unit) > 0);
                        auto c_color = ( c_orient_pos ? Neg_Contour_Color : Pos_Contour_Color );

                        //Override the colour if metadata requests it and we know the colour.
                        if(auto m_color = c.GetMetadataValueAs<std::string>("OutlineColour")){
                            if(auto rgb_c = Colour_from_name(m_color.value())){
                                c_color = sf::Color( static_cast<uint8_t>(rgb_c.value().R * 255.0),
                                                     static_cast<uint8_t>(rgb_c.value().G * 255.0),
                                                     static_cast<uint8_t>(rgb_c.value().B * 255.0) );
                            }
                        }

                        for(auto & p : c.points){
                            //We have three distinct coordinate systems: DICOM, pixel coordinates and screen pixel coordinates,
                            // and SFML 'world' coordinates. We need to map from the DICOM coordinates to screen pixel coords.

                            //Get a DICOM-coordinate bounding box for the image.
                            const auto img_dicom_width = disp_img_it->pxl_dx * disp_img_it->rows;
                            const auto img_dicom_height = disp_img_it->pxl_dy * disp_img_it->columns; 
                            const auto img_top_left = disp_img_it->anchor + disp_img_it->offset
                                                    - disp_img_it->row_unit * disp_img_it->pxl_dx * 0.5f
                                                    - disp_img_it->col_unit * disp_img_it->pxl_dy * 0.5f;
                            const auto img_top_right = img_top_left + disp_img_it->row_unit * img_dicom_width;
                            const auto img_bottom_left = img_top_left + disp_img_it->col_unit * img_dicom_height;
                            
                            //Clamp the point to the bounding box, using the top left as zero.
                            const auto dR = p - img_top_left;
                            const auto clamped_col = dR.Dot( disp_img_it->col_unit ) / img_dicom_height;
                            const auto clamped_row = dR.Dot( disp_img_it->row_unit ) / img_dicom_width;

                            //Convert to SFML coordinates using the SFML bounding box for the display image.
                            sf::FloatRect DispImgBBox = disp_img_texture_sprite.second.getGlobalBounds(); //Uses top left corner as (0,0).
                            const auto world_x = DispImgBBox.left + DispImgBBox.width  * clamped_col;
                            const auto world_y = DispImgBBox.top  + DispImgBBox.height * clamped_row;

                            lines.append( sf::Vertex( sf::Vector2f(world_x, world_y), c_color ) );
                        }
                        if(lines.getVertexCount() != 0) lines.append( lines[0] );
                        window.draw(lines);

                        //Check if the mouse is within the contour. If so, display the name.
                        const sf::Vector2i mouse_coords = sf::Mouse::getPosition(window);
                        //--------------------
                        sf::Vector2f mouse_world_pos = window.mapPixelToCoords(mouse_coords);
                        sf::FloatRect DispImgBBox = disp_img_texture_sprite.second.getGlobalBounds();
                        if(DispImgBBox.contains(mouse_world_pos)){
                            //Assuming the image is not rotated or skewed (though possibly scaled), determine which image pixel
                            // we are hovering over.

                            //Get a DICOM-coordinate bounding box for the image.
                            const auto img_dicom_width = disp_img_it->pxl_dx * disp_img_it->rows;
                            const auto img_dicom_height = disp_img_it->pxl_dy * disp_img_it->columns; 
                            const auto img_top_left = disp_img_it->anchor + disp_img_it->offset
                                                    - disp_img_it->row_unit * disp_img_it->pxl_dx * 0.5f
                                                    - disp_img_it->col_unit * disp_img_it->pxl_dy * 0.5f;
                            const auto img_top_right = img_top_left + disp_img_it->row_unit * img_dicom_width;
                            const auto img_bottom_left = img_top_left + disp_img_it->col_unit * img_dicom_height;

                            const auto clamped_col_as_f = fabs(mouse_world_pos.x - DispImgBBox.left)/(DispImgBBox.width);
                            const auto clamped_row_as_f = fabs(DispImgBBox.top - mouse_world_pos.y)/(DispImgBBox.height);

                            const auto dicom_pos = img_top_left 
                                                 + disp_img_it->row_unit * img_dicom_width  * clamped_row_as_f
                                                 + disp_img_it->col_unit * img_dicom_height * clamped_col_as_f;

                            const auto img_plane = disp_img_it->image_plane();
                            if(c.Is_Point_In_Polygon_Projected_Orthogonally(img_plane,dicom_pos)){
                                auto ROINameOpt = c.GetMetadataValueAs<std::string>("ROIName");
                                auto NormROINameOpt = c.GetMetadataValueAs<std::string>("NormalizedROIName");
                                contourtextss << (NormROINameOpt ? NormROINameOpt.value() : "???");
                                contourtextss << " --- "; 
                                contourtextss << (ROINameOpt ? ROINameOpt.value() : "???");
                                contourtextss << std::endl; 
                            }
                        //--------------------
                        }
                    }
                }
            }

            contourtext.setString(contourtextss.str());
            const auto item_bbox = contourtext.getGlobalBounds();
            const auto item_trc  = sf::Vector2f( item_bbox.left + item_bbox.width, item_bbox.top );

            const auto wndw_view = window.getView();
            const auto view_cntr = wndw_view.getCenter();
            const auto view_size = wndw_view.getSize();
            const auto view_trc  = sf::Vector2f( view_cntr.x + 0.48f*view_size.x, view_cntr.y - 0.48f*view_size.y );

            //We should have that the item's bottom right corner coincides with the window's bottom right corner.
            const sf::Vector2f offset = view_trc - item_trc;

            //Now move the text over.
            contourtext.move(offset);
            window.draw(contourtext);
        }

        //Draw any contours from the contouring buffer that lie in the plane of the current image.
        {
            for(auto & c : contour_coll_shtl.contours){
                if(!c.points.empty() 
                && ( 
                      // Permit contours with any included vertices or at least the 'centre' within the image.
                      ( disp_img_it->sandwiches_point_within_top_bottom_planes(c.Average_Point())
                        || disp_img_it->encompasses_any_of_contour_of_points(c) )
                      || 
                      ( disp_img_it->pxl_dz <= std::numeric_limits<double>::min() ) // //Permit contours on purely 2D images.
                   ) ){
                    sf::VertexArray lines;
                    lines.setPrimitiveType(sf::LinesStrip);

                    for(auto & p : c.points){
                        //We have three distinct coordinate systems: DICOM, pixel coordinates and screen pixel coordinates,
                        // and SFML 'world' coordinates. We need to map from the DICOM coordinates to screen pixel coords.

                        //Get a DICOM-coordinate bounding box for the image.
                        const auto img_dicom_width = disp_img_it->pxl_dx * disp_img_it->rows;
                        const auto img_dicom_height = disp_img_it->pxl_dy * disp_img_it->columns; 
                        const auto img_top_left = disp_img_it->anchor + disp_img_it->offset
                                                - disp_img_it->row_unit * disp_img_it->pxl_dx * 0.5f
                                                - disp_img_it->col_unit * disp_img_it->pxl_dy * 0.5f;
                        const auto img_top_right = img_top_left + disp_img_it->row_unit * img_dicom_width;
                        const auto img_bottom_left = img_top_left + disp_img_it->col_unit * img_dicom_height;
                        
                        //Clamp the point to the bounding box, using the top left as zero.
                        const auto dR = p - img_top_left;
                        const auto clamped_col = dR.Dot( disp_img_it->col_unit ) / img_dicom_height;
                        const auto clamped_row = dR.Dot( disp_img_it->row_unit ) / img_dicom_width;

                        //Convert to SFML coordinates using the SFML bounding box for the display image.
                        sf::FloatRect DispImgBBox = disp_img_texture_sprite.second.getGlobalBounds(); //Uses top left corner as (0,0).
                        const auto world_x = DispImgBBox.left + DispImgBBox.width  * clamped_col;
                        const auto world_y = DispImgBBox.top  + DispImgBBox.height * clamped_row;

                        lines.append( sf::Vertex( sf::Vector2f(world_x, world_y), Editing_Contour_Color ) );
                    }
                    if(lines.getVertexCount() != 0) lines.append( lines[0] );
                    window.draw(lines);
                }
            }
        }


        window.display();

        if(DumpScreenshot){
            DumpScreenshot = false;
            const auto fname_sshot = Get_Unique_Sequential_Filename("/tmp/DICOMautomaton_screenshot_",6,".png");

            auto windowSize = window.getSize();
            sf::Texture texture;
            texture.create(windowSize.x, windowSize.y);
            texture.update(window);
            sf::Image screenshot = texture.copyToImage();
            if(!screenshot.saveToFile(fname_sshot)){
                FUNCWARN("Unable to dump screenshot to file '" << fname_sshot << "'");
            }

        }
        if(SingleScreenshot){
            if((--SingleScreenshotCounter) <= 0){
                const auto fname_sshot = (SingleScreenshotFileName.empty()) ? Get_Unique_Sequential_Filename("/tmp/DICOMautomaton_singlescreenshot_",6,".png")
                                                                            : SingleScreenshotFileName;
                auto windowSize = window.getSize();
                sf::Texture texture;
                texture.create(windowSize.x, windowSize.y);
                texture.update(window);
                sf::Image screenshot = texture.copyToImage();
                if(!screenshot.saveToFile(fname_sshot)){
                    FUNCWARN("Unable to dump screenshot to file '" << fname_sshot << "'");
                }

                window.close();
                break;
            }
        }

        // -------------------------------- Plotting Window Rendering ----------------------------------------

        do{
            if(!plotwindow.isOpen()) break;
            if(!window.hasFocus()) break;
        
            plotwindow.clear(sf::Color::Black);

            // ---- Get time course data for the current pixel, if available ----
            auto mc = Convert_Mouse_Coords();
            if(!mc.voxel_DICOM_pos_valid){
                FUNCWARN("The mouse is not currently hovering over the image. Cannot place contour vertex");
                break;
            }
            const auto row_as_u = mc.pixel_image_pos_row;
            const auto col_as_u = mc.pixel_image_pos_col;
            const auto pix_pos = mc.voxel_DICOM_pos;
            const auto clamped_row_as_f = mc.clamped_image_pos.x;
            const auto clamped_col_as_f = mc.clamped_image_pos.y;

            //Get a list of images which spatially overlap this point. Order should be maintained.
            const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
            const std::list<vec3<double>> points = { pix_pos, pix_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                              pix_pos - ortho * disp_img_it->pxl_dz * 0.25 };
            auto encompassing_images = (*img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);

            //Cycle over the images, dumping the ordinate (pixel values) vs abscissa (time) derived from metadata.
            samples_1D<double> shtl;
            const bool sort_on_append = false;
            
            if(false){
            }else if(plotwindowtype == SecondaryPlot::TimeCourse){
                const std::string quantity("dt"); //As it appears in the metadata. Must convert to a double!

                for(const auto &enc_img_it : encompassing_images){
                    if(auto abscissa = enc_img_it->GetMetadataValueAs<double>(quantity)){
                        try{
                            const auto pix_val = enc_img_it->value(pix_pos,0);
                            shtl.push_back( abscissa.value(), 0.0, 
                                            static_cast<double>(pix_val), 0.0, sort_on_append );
                        }catch(const std::exception &){ }
                    }
                }

            }else if(plotwindowtype == SecondaryPlot::ColumnProfile){
                for(auto i = 0; i < disp_img_it->rows; ++i){
                    const auto val_raw = disp_img_it->value(i,col_as_u,0);
                    const auto row_num = static_cast<double>(i);
                    if(std::isfinite(val_raw)) shtl.push_back({ row_num, 0.0, val_raw, 0.0 });
                }

            }else if(plotwindowtype == SecondaryPlot::RowProfile){
                for(auto i = 0; i < disp_img_it->columns; ++i){
                    const auto val_raw = disp_img_it->value(row_as_u,i,0);
                    const auto col_num = static_cast<double>(i);
                    if(std::isfinite(val_raw)) shtl.push_back({ col_num, 0.0, val_raw, 0.0 });
                }

            }
            shtl.stable_sort();

            if(shtl.size() < 2) break;

            // Draw the plot.
            {
                //Get data extrema.
                const auto D_Hminmax = shtl.Get_Extreme_Datum_x();
                const auto D_Vminmax = shtl.Get_Extreme_Datum_y();

                const auto D_MinH = D_Hminmax.first[0];
                const auto D_MaxH = D_Hminmax.second[0];
                const auto D_MinV = D_Vminmax.first[2];
                const auto D_MaxV = D_Vminmax.second[2];

                const auto D_H = std::abs(D_MaxH - D_MinH);
                const auto D_V = std::abs(D_MaxV - D_MinV);

                if(  ( D_H < std::numeric_limits<float>::min() )
                  || ( D_V < std::numeric_limits<float>::min() ) ){
                    //Too small to plot...
                    plotwindow.clear(sf::Color::Black);
                    break;
                }


                //Get SFML window extrema. Remember Top-left screen corner is origin, so +y is DOWNWARD.
                const auto V = plotwindow.getView();
                const auto V_S = V.getSize();
                const auto V_P = V.getCenter();

                const auto V_MinH = V_P.x - 0.5 * V_S.x;
                const auto V_MaxH = V_MinH + V_S.x;
                const auto V_MinV = V_P.y - 0.5 * V_S.y;
                const auto V_MaxV = V_MinV + V_S.y;

                const auto V_H = std::abs(V_MaxH - V_MinH);
                const auto V_V = std::abs(V_MaxV - V_MinV);

                //Create a lambda function that maps from the data space to the SFML space.
                const auto D_to_V_map_H = [=](double x) -> float {
                    return static_cast<float>( (x - D_MinH) * V_H / D_H - V_MinH );
                };
                const auto D_to_V_map_V = [=](double y) -> float {
                    return static_cast<float>( (D_MaxV - y) * V_V / D_V - V_MinV );
                };

                //Plot axes lines.
                {
                    std::vector<sf::Vertex> H_axes;
                    std::vector<sf::Vertex> V_axes;

                    H_axes.emplace_back( sf::Vector2f( D_to_V_map_H(0.0), D_to_V_map_V(D_MinV) ), sf::Color::Blue );
                    H_axes.emplace_back( sf::Vector2f( D_to_V_map_H(0.0), D_to_V_map_V(D_MaxV) ), sf::Color::Blue );

                    V_axes.emplace_back( sf::Vector2f( D_to_V_map_H(D_MinH), D_to_V_map_V(0.0) ), sf::Color::Blue );
                    V_axes.emplace_back( sf::Vector2f( D_to_V_map_H(D_MaxH), D_to_V_map_V(0.0) ), sf::Color::Blue );

                    plotwindow.draw(H_axes.data(), H_axes.size(), sf::PrimitiveType::LineStrip);
                    plotwindow.draw(V_axes.data(), V_axes.size(), sf::PrimitiveType::LineStrip);
                }

                //Plot where the mouse is (row or column).
                {
                    if(plotwindowtype == SecondaryPlot::RowProfile){
                        std::vector<sf::Vertex> mouse_line;
                        mouse_line.emplace_back( sf::Vector2f( D_to_V_map_H(D_MinH + clamped_col_as_f * D_H), D_to_V_map_V(D_MinV) ), sf::Color::Blue );
                        mouse_line.emplace_back( sf::Vector2f( D_to_V_map_H(D_MinH + clamped_col_as_f * D_H), D_to_V_map_V(D_MaxV) ), sf::Color::Blue );
                        plotwindow.draw(mouse_line.data(), mouse_line.size(), sf::PrimitiveType::LineStrip);
                    }

                    if(plotwindowtype == SecondaryPlot::ColumnProfile){
                        std::vector<sf::Vertex> mouse_line;
                        mouse_line.emplace_back( sf::Vector2f( D_to_V_map_H(D_MinH + clamped_row_as_f * D_H), D_to_V_map_V(D_MinV) ), sf::Color::Blue );
                        mouse_line.emplace_back( sf::Vector2f( D_to_V_map_H(D_MinH + clamped_row_as_f * D_H), D_to_V_map_V(D_MaxV) ), sf::Color::Blue );
                        plotwindow.draw(mouse_line.data(), mouse_line.size(), sf::PrimitiveType::LineStrip);
                    }
                }

                //Plot the data.
                {
                    //Plot lines connecting the vertices.
                    {
                        std::vector<sf::Vertex> verts;
                        for(const auto & d : shtl.samples){
                            verts.emplace_back( sf::Vector2f( D_to_V_map_H(d[0]), D_to_V_map_V(d[2]) ), sf::Color::Red );
                        }
                        plotwindow.draw(verts.data(), verts.size(), sf::PrimitiveType::LineStrip);
                    }
                    
                    /*
                    //Plot circles at the vertices. This can be slow for even 1024 pixels.
                    const float r = 2.0f;
                    const size_t pc = 10; // Number of triangles to use to approximate each circle.
                    std::vector<sf::CircleShape> circs;
                    for(const auto & d : shtl.samples){
                        circs.emplace_back();
                        circs.back().setPointCount(pc);
                        circs.back().setRadius(r);
                        circs.back().setFillColor(sf::Color(255,69,0,255)); // Orange.
                        circs.back().setOutlineColor(sf::Color::Yellow);
                        circs.back().setOutlineThickness(1.0f);
                        circs.back().setPosition( sf::Vector2f( D_to_V_map_H(d[0]), D_to_V_map_V(d[2]) ) );
                        //const auto circ_bb = circs.back().getGlobalBounds();
                        //circs.back().move( sf::Vector2f( -0.5f * circ_bb.width, -0.5f * circ_bb.height ) ); // Translate centre of circle to point of interest.
                    }
                    for(auto &acirc : circs) plotwindow.draw(acirc);
                    */

                    //Plot vertices as a single vertex.
                    {
                        std::vector<sf::Vertex> verts;
                        for(const auto & d : shtl.samples){
                            verts.emplace_back( sf::Vector2f( D_to_V_map_H(d[0]), D_to_V_map_V(d[2]) ), sf::Color::Yellow );
                        }
                        plotwindow.draw(verts.data(), verts.size(), sf::PrimitiveType::Points);
                    }

                }

            }

            plotwindow.display();

        }while(false);

    }

    return DICOM_data;
}
