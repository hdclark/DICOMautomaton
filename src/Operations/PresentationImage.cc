//PresentationImage.cc - A part of DICOMautomaton 2015 - 2018. Written by hal clark.

#include <SFML/Graphics.hpp>
#include <boost/filesystem.hpp>
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

#include "../Colour_Maps.h"
#include "../Common_Boost_Serialization.h"
#include "../Common_Plotting.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "PresentationImage.h"
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocPresentationImage(){
    OperationDoc out;
    out.name = "PresentationImage";
    out.desc = "This operation renders an image with any contours in-place and colour mapping using an SFML backend.";


    out.notes.emplace_back(
      "By default this operation displays the last available image. This makes it easier to produce a sequence of"
      " images by inserting this operation into a sequence of operations."
    );

    out.args.emplace_back();
    out.args.back().name = "ScaleFactor";
    out.args.back().desc = " This factor is applied to the image width and height to magnify (larger than 1) or shrink"
                      " (less than 1) the image. This factor only affects the output image size."
                      " Note that aspect ratio is retained, but rounding for non-integer factors may lead to small (1-2"
                      " pixel) discrepancies.";
    out.args.back().default_val = "1.0";
    out.args.back().expected = true;
    out.args.back().examples = { "0.5", "1.0", "2.0", "5.23" };

    out.args.emplace_back();
    out.args.back().name = "ImageFileName";
    out.args.back().desc = " The file name to use for the image."
                      " If blank, a filename will be generated sequentially.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/an_image.png", "afile.png" };
    out.args.back().mimetype = "image/png";

    out.args.emplace_back();
    out.args.back().name = "ColourMapRegex";
    out.args.back().desc = " The colour mapping to apply to the image if there is a single channel."
                           " The default will match the first available, and if there is no matching"
                           " map found, the first available will be selected.";
    out.args.back().default_val = ".*";
    out.args.back().expected = true;
    out.args.back().examples = { "Viridis",
                                 "Magma",
                                 "Plasma",
                                 "Inferno",
                                 "Jet",
                                 "MorelandBlueRed",
                                 "MorelandBlackBody",
                                 "MorelandExtendedBlackBody",
                                 "KRC",
                                 "ExtendedKRC",
                                 "Kovesi_LinKRYW_5-100_c64",
                                 "Kovesi_LinKRYW_0-100_c71",
                                 "Kovesi_Cyclic_cet-c2",
                                 "LANLOliveGreentoBlue",
                                 "YgorIncandescent",
                                 "LinearRamp" };

    out.args.emplace_back();
    out.args.back().name = "WindowLow";
    out.args.back().desc = "If provided, this parameter will override any existing window and level."
                           " All pixels with the intensity value or lower will be assigned the lowest"
                           " possible colour according to the colour map."
                           " Not providing a valid number will disable window overrides.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "", "-1.23", "0", "1E4" };

    out.args.emplace_back();
    out.args.back().name = "WindowHigh";
    out.args.back().desc = "If provided, this parameter will override any existing window and level."
                           " All pixels with the intensity value or higher will be assigned the highest"
                           " possible colour according to the colour map."
                           " Not providing a valid number will disable window overrides.";
    out.args.back().default_val = "";
    out.args.back().expected = false;
    out.args.back().examples = { "", "1.23", "0", "10.3E4" };

    return out;
}

Drover PresentationImage( Drover DICOM_data, 
                    OperationArgPkg OptArgs, 
                    std::map<std::string,std::string> /*InvocationMetadata*/, 
                    std::string /*FilenameLex*/ ){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto ImageFileName = OptArgs.getValueStr("ImageFileName").value();
    auto ScaleFactor = std::stod( OptArgs.getValueStr("ScaleFactor").value() );
    auto ColourMapRegexStr = OptArgs.getValueStr("ColourMapRegex").value();

    auto WindowLowOpt = OptArgs.getValueStr("WindowLow");
    auto WindowHighOpt = OptArgs.getValueStr("WindowHigh");

    //-----------------------------------------------------------------------------------------------------------------
    const auto regex_cm = Compile_Regex(ColourMapRegexStr);
    //-----------------------------------------------------------------------------------------------------------------

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
    auto img_array_ptr_it   = img_array_ptr_last;

    //At the moment, we keep a single 'display' image active at a time. To help walk through the neighbouring images
    // (and the rest of the images, for that matter) we keep a container iterator to the image.
    using disp_img_it_t = decltype(DICOM_data.image_data.front()->imagecoll.images.begin());
    auto disp_img_beg  = (*img_array_ptr_it)->imagecoll.images.begin();
    auto disp_img_end  = (*img_array_ptr_it)->imagecoll.images.end();
    auto disp_img_last = std::prev((*img_array_ptr_it)->imagecoll.images.end());

    //Find the image closest to (0,0,0), which is frequently the portion of interest.
    using disp_img_t = decltype(*disp_img_beg);
    auto disp_img_it = std::min_element(disp_img_beg, disp_img_end, 
                    [](disp_img_t &L, disp_img_t &R) -> bool {
                        const auto zero = vec3<double>().zero();
                        return (L.center().sq_dist(zero) < R.center().sq_dist(zero));
                    });

    //Because SFML requires us to keep a sf::Texture alive for the duration of a sf::Sprite, we simply package them
    // together into a texture-sprite bundle. This means a single sf::Sprite must be accompanied by a sf::Texture, 
    // though in general several sf::Sprites could be bound to a sf::Texture.
    typedef std::pair<sf::Texture,sf::Sprite> disp_img_texture_sprite_t;
    disp_img_texture_sprite_t disp_img_texture_sprite;

    //Real-time modifiable sticky window and level.
    std::optional<double> custom_width;
    std::optional<double> custom_centre;

    // Adjust the window and level if the user has specified to do so.
    if(WindowLowOpt && WindowHighOpt){
        try{
            const std::string low_str = WindowLowOpt.value();
            const std::string high_str = WindowHighOpt.value();

            // Parse the values and protect against mixing low and high values.
            const auto new_low  = std::stod(low_str);
            const auto new_high = std::stod(high_str);
            const auto new_fullwidth = std::abs(new_high - new_low);
            const auto new_centre = std::min(new_low, new_high) + 0.5 * new_fullwidth;
            custom_width.emplace(new_fullwidth);
            custom_centre.emplace(new_centre);
        }catch(const std::exception &e){
            throw std::runtime_error("Unable to parse window and level: '"_s + e.what() + "'");
        }
    }

    //Open a window.
    sf::RenderTexture window;
    {
        const auto ImagePixelAspectRatio = disp_img_it->pxl_dx / disp_img_it->pxl_dy;
        const auto render_img_w = static_cast<uint32_t>( ScaleFactor * static_cast<double>(disp_img_it->columns) * ImagePixelAspectRatio );
        const auto render_img_h = static_cast<uint32_t>( ScaleFactor * static_cast<double>(disp_img_it->rows) );
        if(!window.create( render_img_w, render_img_h )){
            throw std::runtime_error("Unable to create the render window. Cannot continue.");
        }
    }

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

        std::make_pair("LinearRamp", ColourMap_Linear)
    };
    size_t colour_map = 0;
    
    // Find the requested map, if one is specified.
    for(size_t i = 0; i < colour_maps.size(); ++i){
        if(std::regex_match(colour_maps[i].first, regex_cm)){
            colour_map = i;
            break;
        }
    }

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
                        const auto rescaled_value = (clamped_value - clamped_low)/(clamped_high - clamped_low);

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
    const auto scale_sprite_to_fill_screen = [](const sf::RenderTexture &awindow, 
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

    // Prep the first image.
    if(!load_img_texture_sprite(disp_img_it, disp_img_texture_sprite)){
        FUNCERR("Unable to load image --> texture --> sprite");
    }
    scale_sprite_to_fill_screen(window,disp_img_it,disp_img_texture_sprite);


    // -------------------------------------- Rendering ----------------------------------------

    window.clear(sf::Color::Black);
    window.draw(disp_img_texture_sprite.second);

    //Draw any contours that lie in the plane of the current image. Also draw contour names if the cursor is 'within' them.
    if(DICOM_data.contour_data != nullptr){
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
                    const auto c_orient = c.Estimate_Planar_Normal();
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
                }
            }
        }
    }

    const auto fname = (ImageFileName.empty()) ? Get_Unique_Sequential_Filename("/tmp/DICOMautomaton_presentationimage_",6,".png")
                                               : ImageFileName;

    window.display(); // Required, even though nothing is displayed on the screen.
    window.getTexture().copyToImage().saveToFile(fname);

    return DICOM_data;
}
