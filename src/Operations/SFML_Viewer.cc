//SFML_Viewer.cc - A part of DICOMautomaton 2015, 2016. Written by hal clark.

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>    
#include <vector>
#include <set> 
#include <map>
#include <unordered_map>
#include <list>
#include <functional>
#include <thread>
#include <array>
#include <mutex>
#include <limits>
#include <cmath>
#include <regex>

#include <getopt.h>           //Needed for 'getopts' argument parsing.
#include <cstdlib>            //Needed for exit() calls.
#include <utility>            //Needed for std::pair.
#include <algorithm>
#include <experimental/optional>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <pqxx/pqxx>          //PostgreSQL C++ interface.

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathPlottingGnuplot.h" //Needed for YgorMathPlottingGnuplot::*.
#include "YgorMathChebyshev.h" //Needed for cheby_approx class.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorContainers.h"   //Needed for bimap class.
#include "YgorPerformance.h"  //Needed for YgorPerformance_dt_from_last().
#include "YgorAlgorithms.h"   //Needed for For_Each_In_Parallel<..>(...)
#include "YgorArguments.h"    //Needed for ArgumentHandler class.
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"
#include "YgorImagesIO.h"
#include "YgorImagesPlotting.h"

#include "Explicator.h"       //Needed for Explicator class.

#include "../Structs.h"
#include "../StructsIOBoostSerialization.h"

#include "../Common_Boost_Serialization.h"
#include "../Common_Plotting.h"

#include "../YgorImages_Functors/Grouping/Misc_Functors.h"

#include "../YgorImages_Functors/Processing/DCEMRI_AUC_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_S0_Map.h"
#include "../YgorImages_Functors/Processing/DCEMRI_T1_Map.h"
#include "../YgorImages_Functors/Processing/Highlight_ROI_Voxels.h"
#include "../YgorImages_Functors/Processing/Kitchen_Sink_Analysis.h"
#include "../YgorImages_Functors/Processing/IVIMMRI_ADC_Map.h"
#include "../YgorImages_Functors/Processing/Time_Course_Slope_Map.h"
#include "../YgorImages_Functors/Processing/CT_Perfusion_Clip_Search.h"
#include "../YgorImages_Functors/Processing/CT_Perf_Pixel_Filter.h"
#include "../YgorImages_Functors/Processing/CT_Convert_NaNs_to_Air.h"
#include "../YgorImages_Functors/Processing/Min_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/Max_Pixel_Value.h"
#include "../YgorImages_Functors/Processing/CT_Reasonable_HU_Window.h"
#include "../YgorImages_Functors/Processing/Slope_Difference.h"
#include "../YgorImages_Functors/Processing/Centralized_Moments.h"
#include "../YgorImages_Functors/Processing/Logarithmic_Pixel_Scale.h"
#include "../YgorImages_Functors/Processing/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Processing/DBSCAN_Time_Courses.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bilinear_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Bicubic_Supersample.h"
#include "../YgorImages_Functors/Processing/In_Image_Plane_Pixel_Decimate.h"
#include "../YgorImages_Functors/Processing/Cross_Second_Derivative.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_Common.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_Chebyshev_FreeformOptimization.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_Chebyshev_LevenbergMarquardt.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_Reduced3Param_Chebyshev_Common.h"
#include "../YgorImages_Functors/Processing/Liver_Kinetic_1Compartment2Input_Reduced3Param_Chebyshev_FreeformOptimization.h"
#include "../YgorImages_Functors/Processing/Orthogonal_Slices.h"

#include "../YgorImages_Functors/Transform/DCEMRI_C_Map.h"
#include "../YgorImages_Functors/Transform/DCEMRI_Signal_Difference_C.h"
#include "../YgorImages_Functors/Transform/CT_Perfusion_Signal_Diff.h"
#include "../YgorImages_Functors/Transform/DCEMRI_S0_Map_v2.h"
#include "../YgorImages_Functors/Transform/DCEMRI_T1_Map_v2.h"
#include "../YgorImages_Functors/Transform/Pixel_Value_Histogram.h"
#include "../YgorImages_Functors/Transform/Subtract_Spatially_Overlapping_Images.h"

#include "../YgorImages_Functors/Compute/Per_ROI_Time_Courses.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"

#include "../KineticModel_1Compartment2Input_5Param_Chebyshev_Common.h"
#include "../KineticModel_1Compartment2Input_5Param_Chebyshev_FreeformOptimization.h"
#include "../KineticModel_1Compartment2Input_5Param_Chebyshev_LevenbergMarquardt.h"

#include "../KineticModel_1Compartment2Input_5Param_LinearInterp_Common.h"
#include "../KineticModel_1Compartment2Input_5Param_LinearInterp_LevenbergMarquardt.h"

#include "SFML_Viewer.h"


std::list<OperationArgDoc> OpArgDocSFML_Viewer(void){
    return std::list<OperationArgDoc>();
}

Drover SFML_Viewer(Drover DICOM_data, OperationArgPkg /*OptArgs*/, std::map<std::string,std::string> /*InvocationMetadata*/, std::string /*FilenameLex*/){

    //Trim any empty image sets.
    for(auto it = DICOM_data.image_data.begin(); it != DICOM_data.image_data.end();  ){
        if((*it)->imagecoll.images.empty()){
            it = DICOM_data.image_data.erase(it);
        }else{
            ++it;
        }
    }
    if(DICOM_data.image_data.empty()) FUNCERR("No image data available to view. Cannot continue");


    //Produce a little sound to notify the user we've started showing something.
    sf::Music music;
    {
        const std::vector<std::string> soundpaths = { "Sounds/Ready.ogg",
           "/home/hal/Dropbox/Project - DICOMautomaton/Sounds/Ready.ogg",
           "/tmp/Ready.ogg", "Ready.ogg" };
        bool worked = false;
        for(const auto &soundpath : soundpaths){
            if(!music.openFromFile(soundpath)) continue;
            worked = true;
            music.play();
            break;
        }
        if(!worked){
            FUNCWARN("Unable to play notification sound. Continuing anyways");
        }
    }

    //If, for some reason, several image arrays are available for viewing, we need to provide a means for stepping
    // through the arrays. 
    //
    // NOTE: The reasoning for having several image arrays is not clear cut. If the timestamps are known exactly, it
    //       might make sense to split in this way. In general, it is up to the user to make this call. 
    typedef decltype(DICOM_data.image_data.begin()) img_data_ptr_it_t;
    img_data_ptr_it_t img_array_ptr_beg  = DICOM_data.image_data.begin();
    img_data_ptr_it_t img_array_ptr_end  = DICOM_data.image_data.end();
    img_data_ptr_it_t img_array_ptr_last = std::prev(DICOM_data.image_data.end());
    img_data_ptr_it_t img_array_ptr_it   = img_array_ptr_beg;

    //At the moment, we keep a single 'display' image active at a time. To help walk through the neighbouring images
    // (and the rest of the images, for that matter) we keep a container iterator to the image.
    typedef decltype(DICOM_data.image_data.front()->imagecoll.images.begin()) disp_img_it_t;
    disp_img_it_t disp_img_beg  = (*img_array_ptr_it)->imagecoll.images.begin();
    //disp_img_it_t disp_img_end  = (*img_array_ptr_it)->imagecoll.images.end();  // Not used atm.
    disp_img_it_t disp_img_last = std::prev((*img_array_ptr_it)->imagecoll.images.end());
    disp_img_it_t disp_img_it   = disp_img_beg;

    //Because SFML requires us to keep a sf::Texture alive for the duration of a sf::Sprite, we simply package them
    // together into a texture-sprite bundle. This means a single sf::Sprite must be accompanied by a sf::Texture, 
    // though in general several sf::Sprites could be bound to a sf::Texture.
    typedef std::pair<sf::Texture,sf::Sprite> disp_img_texture_sprite_t;
    disp_img_texture_sprite_t disp_img_texture_sprite;

    //Real-time modifiable sticky window and level.
    std::experimental::optional<double> custom_width;
    std::experimental::optional<double> custom_centre;

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
    window.setFramerateLimit(60);

    //Create a secondary plotting window. Gets opened on command.
    sf::RenderWindow plotwindow;

    if(auto ImageDesc = disp_img_it->GetMetadataValueAs<std::string>("Description")){
        window.setTitle("DICOMautomaton IV: '"_s + ImageDesc.value() + "'");
    }else{
        window.setTitle("DICOMautomaton IV: <no description available>");
    }

    //Attempt to load fonts. We should try a few different files, and include a back-up somewhere accessible...
    sf::Font afont;
    if(!afont.loadFromFile("/usr/share/fonts/TTF/cmr10.ttf")) FUNCERR("Unable to find font file");

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
    cursortext.setColor(sf::Color::Green);

    sf::Text BRcornertext;
    std::stringstream BRcornertextss;
    BRcornertext.setFont(afont);
    BRcornertext.setString("");
    BRcornertext.setCharacterSize(9); //Size in pixels, not in points.
    BRcornertext.setColor(sf::Color::Red);

    sf::Text BLcornertext;
    std::stringstream BLcornertextss;
    BLcornertext.setFont(afont);
    BLcornertext.setString("");
    BLcornertext.setCharacterSize(15); //Size in pixels, not in points.
    BLcornertext.setColor(sf::Color::Blue);

    

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
            const double destmin = static_cast<double>( 0 );
            const double destmax = static_cast<double>( std::numeric_limits<uint8_t>::max() );
    
            for(auto i = 0; i < img_cols; ++i){
                for(auto j = 0; j < img_rows; ++j){
                    const auto val = static_cast<double>( img_it->value(j,i,0) ); //The first (R or gray) channel.
                    if(!std::isfinite(val)){
                        animage.setPixel(i,j,sf::Color(255,0,0));
                    }else{
                        double y = destmin; //The new value of the pixel.
    
                        //If above or below the cutoff range, the pixel could be treated as the window min/max or simply as if
                        // it did not exist. We could set the value to fully transparent, NaN, or a specially designated 
                        // 'ignore' colour. It's ultimately up to the user.
                        if(val <= (win_c - win_r)){
                            y = destmin;
                        }else if(val >= (win_c + win_r)){
                            //Logical choice, but make viewing hard if window is too low...
                            y = destmax;
    
                        //If within the window range. Scale linearly as the pixel's position in the window.
                        }else{
                            //const double clamped = (val - (win_c - win_r)) / (2.0*win_r + 1.0);
                            const double clamped = (val - (win_c - win_r)) / win_fw;
                            y = clamped * (destmax - destmin) + destmin;
                        }
    
                        const auto scaled_value = static_cast<uint8_t>( std::floor(y) );
                        animage.setPixel(i,j,sf::Color(scaled_value,scaled_value,scaled_value));
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
            typedef decltype(img_it->value(0,0,0)) pixel_value_t;
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
                        animage.setPixel(i,j,sf::Color(255,0,0));
                    }else{
                        const double clamped_value = (static_cast<double>(val) - pixel_type_min)/(pixel_type_max - pixel_type_min);
                        const auto rescaled_value = (clamped_value - clamped_low)/(clamped_high - clamped_low);
                        const auto scaled_value = static_cast<uint8_t>(rescaled_value * dest_type_max);
                        animage.setPixel(i,j,sf::Color(scaled_value,scaled_value,scaled_value));
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


    //Run until the window is closed or the user wishes to exit.
    while(window.isOpen()){
        BRcornertextss.str(""); //Clear stringstream.

        //Check if any events have accumulated since the last poll. If so, deal with them.
        sf::Event event;
        while(window.pollEvent(event)){
            if(event.type == sf::Event::Closed){
                window.close();
            }else if(window.hasFocus() && (event.type == sf::Event::KeyPressed)){
                if(event.key.code == sf::Keyboard::Escape){
                    window.close();
                }

            }else if(window.hasFocus() && (event.type == sf::Event::KeyReleased)){

            }else if(window.hasFocus() && (event.type == sf::Event::TextEntered) 
                                       && (event.text.unicode < 128)){
                //Not the same as KeyPressed + KeyReleased. Think unicode characters, or control keys.
                const auto thechar = static_cast<char>(event.text.unicode);

                if( false ){

                //Show a simple help dialog with some keyboard commands.
                }else if( (thechar == 'h') || (thechar == 'H') ){
                    // Easy way to get list of commands:
                    // `grep -C 3 'thechar == ' src/PETCT_Perfusion_Analysis.cc | grep '//\|thechar'`
                    Execute_Command_In_Pipe(
                            "zenity --info --no-wrap --text=\""
                            "DICOMautomaton Image Viewer\\n\\n"
                            "\\t Commands: \\n"
                            "\\t\\t h,H \\t Display this help.\\n"
                            "\\t\\t x \\t\\t Toggle whether existing contours should be displayed.\\n"
                            "\\t\\t m \\t\\t Invoke minetest to perform contouring for this slice.\\n"
                            "\\t\\t d \\t\\t Dump the window contents as an image after the next render.\\n"
                            "\\t\\t D \\t\\t Dump raw pixels for all spatially overlapping images from the current array (e.g., time courses).\\n"
                            "\\t\\t i \\t\\t Dump the current image to file.\\n"
                            "\\t\\t I \\t\\t Dump all images in the current array to file.\\n"
                            "\\t\\t r,R,c,C \\t Plot pixel intensity profiles along the mouse\\'s current row and column.\\n"
                            "\\t\\t t \\t\\t Plot a time course at the mouse\\'s current row and column.\\n"
                            "\\t\\t T \\t\\t Open a realtime plotting window.\\n"
                            "\\t\\t a,A \\t\\t Plot or dump the pixel values for [a]ll image sets which spatially overlap.\\n"
                            "\\t\\t M   \\t\\t Try plot a pharmacokinetic [M]odel using image map parameters and ROI time courses.\\n"
                            "\\t\\t N,P \\t\\t Advance to the next/previous image series.\\n"
                            "\\t\\t n,p \\t\\t Advance to the next/previous image in this series.\\n"
                            "\\t\\t -,+ \\t\\t Advance to the next/previous image that spatially overlaps this image.\\n"
                            "\\t\\t l,L \\t\\t Reset the image scale to be pixel-for-pixel what is seen on screen.\\n"
                            "\\t\\t u   \\t Toggle showing metadata tags that are identical to the neighbouring image\\'s metadata tags.\\n"
                            "\\t\\t U   \\t Dump and show the current image\\'s metadata.\\n"
                            "\\t\\t e \\t\\t Erase latest non-empty contour. (A single contour.)\\n"
                            "\\t\\t E \\t\\t Empty the current working ROI buffer. (The entire buffer; all contours.)\\n"
                            "\\t\\t s,S \\t\\t Save the current contour collection.\\n"
                            "\\t\\t b \\t\\t Serialize Drover instance (all data) to file.\\n"
                            "\\n\""
                    );

                //Dump a serialization of the current (*entire*) Drover class.
                }else if( thechar == 'b' ){
                    const boost::filesystem::path out_fname("/tmp/boost_serialized_drover.bin.gz");
                    const bool res = Common_Boost_Serialize_Drover(DICOM_data, out_fname);
                    if(res){
                        FUNCINFO("Dumped serialization to file " << out_fname.string());
                    }else{
                        FUNCWARN("Unable dump serialization to file " << out_fname.string());
                    }


                //Toggle whether existing contours should be displayed.
                }else if( thechar == 'x' ){
                    ShowExistingContours = !ShowExistingContours;

                //Invoke minetest to perform contouring for this slice.
                }else if( thechar == 'm' ){
                    try{
                        //Step 0: Create a new contour buffer if needed.
                        if(!contour_coll_shtl.contours.back().points.empty()){
                            contour_coll_shtl.contours.emplace_back();
                            contour_coll_shtl.contours.back().closed = true;
                        }

                        //Step 1: Write current image as a FITS file.
                        //        We want the current window to be present too.
                        disp_img_texture_sprite_t asprite;
                        if(!load_img_texture_sprite(disp_img_it, asprite)){
                            throw std::runtime_error("Unable to load image into sprite with window settings.");
                        }
                        planar_image<uint8_t,double> for_mt;
                        for_mt.init_buffer(disp_img_it->rows, disp_img_it->columns, disp_img_it->channels);
                        for_mt.init_spatial(disp_img_it->pxl_dx, disp_img_it->pxl_dy, disp_img_it->pxl_dz,
                                            disp_img_it->anchor, disp_img_it->offset);
                        for_mt.init_orientation(disp_img_it->row_unit, disp_img_it->col_unit);
                        for_mt.fill_pixels(0);
 
                        sf::Image animage = asprite.first.copyToImage();
                        for(auto i = 0; i < for_mt.columns; ++i){
                            for(auto j = 0; j < for_mt.rows; ++j){
                                const uint8_t rchnl = animage.getPixel(i,j).r;
                                for_mt.reference(j,i,0) = rchnl;
                            }
                        }

                        //Perform a pixel compression before writing to file.
                        for(auto i = 0; i < for_mt.columns; ++i){
                            for(auto j = 0; j < for_mt.rows; ++j){
                                //const auto orig = for_mt.value(j,i,0) * 1.0;
                                //const auto scaled = std::log(orig + 1.0) * 45.8; // Range: [0:254].
                                //for_mt.reference(j,i,0) = static_cast<uint8_t>(scaled);

                                const auto orig = for_mt.value(j,i,0) * 1.0;
                                //const auto scaled = (1.0*orig/8.0) + (7.0*253.0/8.0); // Use only top 1/8 of voxels.
                                const auto scaled = (1.0*orig/4.0) + (3.0*253.0/4.0); // Use only top 1/4 of voxels.
                                //const auto scaled = (2.0*orig/5.0) + (3.0*253.0/5.0); // Use only top 2/5 of voxels.
                                //const auto scaled = (2.0*orig/4.0) + (2.0*253.0/4.0); // Use only top 2/4 of voxels.
                                //const auto scaled = (3.0*orig/4.0) + (1.0*253.0/4.0); // Use only top 3/4 of voxels.
                                for_mt.reference(j,i,0) = static_cast<uint8_t>(scaled);
                            }
                        }

                        const std::string shtl_file("/tmp/minetest_u8_in.fits");
                        if(!WriteToFITS(for_mt,shtl_file)){
                            throw std::runtime_error("Unable to write shuttle FITS file.");
                        }

                        //Step 2: Prepare minetest for faster/easier contouring.
                        //        The following skeleton includes fast, fly, and noclip, and is positioned at
                        //        the ~centre of the image looking slightly north.
                        const std::string rs_res = Execute_Command_In_Pipe( "rsync -L --delete -az "
                                " '/home/hal/Research/2016_ICCR_Voxel_Contouring/20160118-195048_minetest_world_T_skeleton/' "
                                " '/home/hal/.minetest/' ");

                        //Step 3: Invoke minetest.
                        const std::string mt_res = Execute_Command_In_Pipe("minetest 2>&1");

                        //Step 4: Parse the output looking for notable events.
                        //        See journal notes for examples of these logs. Basically look for notation like (-123,45,234).
                        std::vector<std::string> events = SplitStringToVector(mt_res, '\n', 'd');
                        std::vector<std::string> relevant;
                        for(auto &event : events){
                            const auto pos = event.find(" singleplayer digs ");

                            if(std::string::npos != pos){
                                //FUNCINFO("Found relevant event: " << event);
                                const auto l_B = GetFirstRegex(event, R"***(([-0-9]{1,3},[-0-9]{1,3},[-0-9]{1,3}))***");
                                //FUNCINFO("Relevant part: " << l_B);
                                relevant.push_back(l_B); // Should be like "179,-2,210".
                            }
                        }

                        //Step 5: iff reasonable events detected, overwrite the existing slice's working contour.
                        for(const auto &event : relevant){
                            std::stringstream ss(event);
                            long int l_row, l_col, l_height;
                            char dummy;
                            ss >> l_row >> dummy >> l_height >> dummy >> l_col;
                            //FUNCINFO("Parsed (row,col) = (" << l_row << "," << l_col << ")");

                            if( isininc(0,l_row,disp_img_it->rows - 1)
                            &&  isininc(0,l_col,disp_img_it->columns - 1) ){
                                const auto dicom_pos = disp_img_it->position(l_row, l_col);
                                //FUNCINFO("Corresponding DICOM position: " << dicom_pos);
                                auto FrameofReferenceUID = disp_img_it->GetMetadataValueAs<std::string>("FrameofReferenceUID");
                                if(FrameofReferenceUID){
                                    //Record the point in the working contour buffer.
                                    contour_coll_shtl.contours.back().closed = true;
                                    contour_coll_shtl.contours.back().points.push_back( dicom_pos );
                                    contour_coll_shtl.contours.back().metadata["FrameofReferenceUID"] = FrameofReferenceUID.value();
                                }else{
                                    FUNCWARN("Unable to find display image's FrameofReferenceUID. Cannot insert point in contour");
                                }
                            }
                        }


                    }catch(const std::exception &e){
                        FUNCWARN("Unable to contour via minetest: " << e.what());
                    }


                //Set the flag for dumping the window contents as an image after the next render.
                }else if( thechar == 'd' ){
                    DumpScreenshot = true;

                //Dump raw pixels for all spatially overlapping images from the current array.
                // (Useful for dumping time courses.)
                }else if( thechar == 'D' ){

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

                //Dump the current image to file.
                }else if(thechar == 'i'){
                    const auto pixel_dump_filename_out = Get_Unique_Sequential_Filename("/tmp/display_image_dump_",6,".fits");
                    if(WriteToFITS(*disp_img_it,pixel_dump_filename_out)){
                        FUNCINFO("Dumped pixel data for this image to file '" << pixel_dump_filename_out << "'");
                    }else{
                        FUNCWARN("Unable to dump pixel data for this image to file '" << pixel_dump_filename_out << "'");
                    }

                //Dump all images in the current array to file.
                }else if(thechar == 'I'){
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

                //Given the current mouse coordinates, dump pixel intensity profiles along the current row and column.
                //
                }else if( (thechar == 'r') || (thechar == 'R') 
                      ||  (thechar == 'c') || (thechar == 'C') ){
                    //Get the *realtime* current mouse coordinates.
                    const sf::Vector2i curr_m_pos = sf::Mouse::getPosition(window);

                    //Convert to SFML/OpenGL "World" coordinates. 
                    sf::Vector2f curr_m_pos_w = window.mapPixelToCoords(curr_m_pos);

                    //Check if the mouse is within the image bounding box. We can only proceed if it is.
                    sf::FloatRect disp_img_bbox = disp_img_texture_sprite.second.getGlobalBounds();
                    if(!disp_img_bbox.contains(curr_m_pos_w)){
                        FUNCWARN("The mouse is not currently hovering over the image. Cannot dump row/column profiles");
                        break;
                    }

                    //Assuming the image is not rotated or skewed (though possibly scaled), determine which image pixel
                    // we are hovering over.
                    const auto clamped_col_as_f = fabs(curr_m_pos_w.x - disp_img_bbox.left)/disp_img_bbox.width;
                    const auto clamped_row_as_f = fabs(disp_img_bbox.top - curr_m_pos_w.y )/disp_img_bbox.height;

                    const auto img_w_h = disp_img_texture_sprite.first.getSize();
                    const auto col_as_u = static_cast<long int>(clamped_col_as_f * static_cast<float>(img_w_h.x));
                    const auto row_as_u = static_cast<long int>(clamped_row_as_f * static_cast<float>(img_w_h.y));
                    FUNCINFO("Dumping row and column profiles for row,col = " << row_as_u << "," << col_as_u);

                    //Cycle over the images, dumping the pixel value(s) and the 'dt' metadata value, if available.
                    //Single-pixel.
                    samples_1D<double> row_profile, col_profile;
                    std::stringstream title;

                    for(auto i = 0; i < disp_img_it->columns; ++i){
                        const auto val_raw = disp_img_it->value(row_as_u,i,0);
                        const auto col_num = static_cast<double>(i);
                        col_profile.push_back({ col_num, 0.0, val_raw, 0.0 });
                    }
                    for(auto i = 0; i < disp_img_it->rows; ++i){
                        const auto val_raw = disp_img_it->value(i,col_as_u,0);
                        const auto row_num = static_cast<double>(i);
                        row_profile.push_back({ row_num, 0.0, val_raw, 0.0 });
                    }

                    title << "Row and Column profile. (row,col) = (" << row_as_u << "," << col_as_u << ").";
                    try{
                        YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> row_shtl(row_profile, "Row Profile");
                        YgorMathPlottingGnuplot::Shuttle<samples_1D<double>> col_shtl(col_profile, "Col Profile");
                        YgorMathPlottingGnuplot::Plot<double>({row_shtl, col_shtl}, title.str(), "Pixel Index (row or col)", "Pixel Intensity");
                    }catch(const std::exception &e){
                        FUNCWARN("Failed to plot: " << e.what());
                    }

                //Launch/open a realtime plotting window.
                }else if( thechar == 'T' ){
                    plotwindow.create(sf::VideoMode(640, 480), "DICOMautomaton Time Courses");
                    plotwindow.setFramerateLimit(30);

                //Given the current mouse coordinates, dump a time series at the image pixel over all available images
                // which spatially overlap.
                //
                // So this routine dumps a time course at the mouse pixel.
                //
                }else if( thechar == 't' ){
                    //Get the *realtime* current mouse coordinates.
                    const sf::Vector2i curr_m_pos = sf::Mouse::getPosition(window);

                    //Convert to SFML/OpenGL "World" coordinates. 
                    sf::Vector2f curr_m_pos_w = window.mapPixelToCoords(curr_m_pos);

                    //Check if the mouse is within the image bounding box. We can only proceed if it is.
                    sf::FloatRect disp_img_bbox = disp_img_texture_sprite.second.getGlobalBounds();
                    if(!disp_img_bbox.contains(curr_m_pos_w)){
                        FUNCWARN("The mouse is not currently hovering over the image. Cannot dump time course");
                        break;
                    }

                    //Assuming the image is not rotated or skewed (though possibly scaled), determine which image pixel
                    // we are hovering over.
                    const auto clamped_col_as_f = fabs(curr_m_pos_w.x - disp_img_bbox.left)/disp_img_bbox.width;
                    const auto clamped_row_as_f = fabs(disp_img_bbox.top - curr_m_pos_w.y )/disp_img_bbox.height;

                    const auto img_w_h = disp_img_texture_sprite.first.getSize();
                    const auto col_as_u = static_cast<uint32_t>(clamped_col_as_f * static_cast<float>(img_w_h.x));
                    const auto row_as_u = static_cast<uint32_t>(clamped_row_as_f * static_cast<float>(img_w_h.y));
                    FUNCINFO("Dumping time course for row,col = " << row_as_u << "," << col_as_u);

                    //Get the YgorImage (DICOM) pixel coordinates.
                    const auto pix_pos = disp_img_it->position(row_as_u,col_as_u);

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


                //Given the current mouse coordinates, try to show a perfusion model using model parameters from
                // other images. Also show a time course of the raw data for comparison with the model fit.
                //
                }else if( (thechar == 'M') ){
                    //Get the *realtime* current mouse coordinates.
                    const sf::Vector2i curr_m_pos = sf::Mouse::getPosition(window);

                    //Convert to SFML/OpenGL "World" coordinates. 
                    sf::Vector2f curr_m_pos_w = window.mapPixelToCoords(curr_m_pos);

                    //Check if the mouse is within the image bounding box. We can only proceed if it is.
                    sf::FloatRect disp_img_bbox = disp_img_texture_sprite.second.getGlobalBounds();
                    if(!disp_img_bbox.contains(curr_m_pos_w)){
                        FUNCWARN("The mouse is not currently hovering over the image. Cannot dump time course");
                        break;
                    }

                    //Assuming the image is not rotated or skewed (though possibly scaled), determine which image pixel
                    // we are hovering over.
                    const auto clamped_col_as_f = fabs(curr_m_pos_w.x - disp_img_bbox.left)/disp_img_bbox.width;
                    const auto clamped_row_as_f = fabs(disp_img_bbox.top - curr_m_pos_w.y )/disp_img_bbox.height;

                    const auto img_w_h = disp_img_texture_sprite.first.getSize();
                    const auto col_as_u = static_cast<uint32_t>(clamped_col_as_f * static_cast<float>(img_w_h.x));
                    const auto row_as_u = static_cast<uint32_t>(clamped_row_as_f * static_cast<float>(img_w_h.y));

                    //Get the YgorImage (DICOM) pixel coordinates.
                    const auto pix_pos = disp_img_it->position(row_as_u,col_as_u);
                    const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
                    const std::list<vec3<double>> points = { pix_pos, pix_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                                      pix_pos - ortho * disp_img_it->pxl_dz * 0.25 };

                    //Metadata quantities of interest.
                    std::regex  k1A_regex(".*k1A.*",  std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
                    std::regex tauA_regex(".*tauA.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
                    std::regex  k1V_regex(".*k1V.*",  std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
                    std::regex tauV_regex(".*tauV.*", std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);
                    std::regex   k2_regex(".*k2.*",   std::regex::icase | std::regex::nosubs | std::regex::optimize | std::regex::extended);

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
                                        break;
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
                             

                //Given the current mouse coordinates, dump the pixel value for [A]ll image sets which spatially overlap.
                // This routine is useful for debugging problematic pixels, or trying to follow per-pixel calculations.
                //
                // NOTE: This routine finds the pixel nearest to the specified voxel in DICOM space. So if the image was
                //       resampled, you will still be able to track the pixel nearest to the centre. In case only exact
                //       pixels should be tracked, the row and column numbers are spit out; so just filter out pixel
                //       coordinates you don't want.
                //
                }else if( (thechar == 'a') || (thechar == 'A') ){
                    //Get the *realtime* current mouse coordinates.
                    const sf::Vector2i curr_m_pos = sf::Mouse::getPosition(window);

                    //Convert to SFML/OpenGL "World" coordinates. 
                    sf::Vector2f curr_m_pos_w = window.mapPixelToCoords(curr_m_pos);

                    //Check if the mouse is within the image bounding box. We can only proceed if it is.
                    sf::FloatRect disp_img_bbox = disp_img_texture_sprite.second.getGlobalBounds();
                    if(!disp_img_bbox.contains(curr_m_pos_w)){
                        FUNCWARN("The mouse is not currently hovering over the image. Cannot dump time course");
                        break;
                    }

                    //Assuming the image is not rotated or skewed (though possibly scaled), determine which image pixel
                    // we are hovering over.
                    const auto clamped_col_as_f = fabs(curr_m_pos_w.x - disp_img_bbox.left)/disp_img_bbox.width;
                    const auto clamped_row_as_f = fabs(disp_img_bbox.top - curr_m_pos_w.y )/disp_img_bbox.height;

                    const auto img_w_h = disp_img_texture_sprite.first.getSize();
                    const auto col_as_u = static_cast<uint32_t>(clamped_col_as_f * static_cast<float>(img_w_h.x));
                    const auto row_as_u = static_cast<uint32_t>(clamped_row_as_f * static_cast<float>(img_w_h.y));

                    //Get the YgorImage (DICOM) pixel coordinates.
                    const auto pix_pos = disp_img_it->position(row_as_u,col_as_u);
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


               //Advance to the next/previous Image_Array. Also reset necessary display image iterators.
               }else if( (thechar == 'N') || (thechar == 'P') ){
                    //Save the current image position. We will attempt to find the same spot after switching arrays.
                    const auto disp_img_pos = static_cast<size_t>( std::distance(disp_img_beg, disp_img_it) );

                    custom_width  = std::experimental::optional<double>();
                    custom_centre = std::experimental::optional<double>();

                    if(thechar == 'N'){
                        if(img_array_ptr_it == img_array_ptr_last){ //Wrap around forwards.
                            img_array_ptr_it = img_array_ptr_beg;
                        }else{
                            std::advance(img_array_ptr_it, 1);
                        }

                    }else if(thechar == 'P'){
                        if(img_array_ptr_it == img_array_ptr_beg){ //Wrap around backwards.
                            img_array_ptr_it = img_array_ptr_last;
                        }else{
                            std::advance(img_array_ptr_it,-1);
                        }
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

                //Advance to the next/previous display image in the current Image_Array.
                }else if( (thechar == 'n') || (thechar == 'p') ){
                    if(thechar == 'n'){
                        if(disp_img_it == disp_img_last){
                            disp_img_it = disp_img_beg;
                        }else{
                            std::advance(disp_img_it, 1);
                        }
                    }else if(thechar == 'p'){
                        if(disp_img_it == disp_img_beg){
                            disp_img_it = disp_img_last;
                        }else{
                            std::advance(disp_img_it,-1);
                        }
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

                //Step to the next/previous image which spatially overlaps with the current display image.
                }else if( (thechar == '-') || (thechar == '+') || (thechar == '_') || (thechar == '=') ){
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
                        if((thechar == '-') || (thechar == '_')){
                            if(enc_img_it == encompassing_images.begin()){
                                disp_img_it = encompassing_images.back();
                            }else{
                                --enc_img_it;
                                disp_img_it = *enc_img_it;
                            }
                        }else if((thechar == '+') || (thechar == '=')){
                            ++enc_img_it;
                            if(enc_img_it == encompassing_images.end()){
                                disp_img_it = encompassing_images.front();
                            }else{
                                disp_img_it = *enc_img_it;
                            }
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


                //Reset the image scale to be pixel-for-pixel what is seen on screen. (Unless there is a view
                // that has some transformation over on-screen objects.)
                }else if( (thechar == 'l') || (thechar == 'L')){
                    disp_img_texture_sprite.second.setScale(1.0f,1.0f);

                //Toggle showing metadata tags that are identical to the neighbouring image's metadata tags.
                }else if( thechar == 'u' ){
                    OnlyShowTagsDifferentToNeighbours = !OnlyShowTagsDifferentToNeighbours;

                //dump to file and show a pop-up window with the full metadata of the present image.
                }else if( thechar == 'U' ){
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


                //Erase the present contour, or, if empty, the previous non-empty contour. (Not the whole organ.)
                }else if( thechar == 'e' ){
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


                //Erase or Empty the current working contour buffer. 
                }else if( thechar == 'E' ){

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

                //Save the current contour collection.
                }else if( (thechar == 's') || (thechar == 'S') ){

                    try{
                        auto FrameofReferenceUID = disp_img_it->GetMetadataValueAs<std::string>("FrameofReferenceUID");
                        auto StudyInstanceUID    = disp_img_it->GetMetadataValueAs<std::string>("StudyInstanceUID");
                        if(!FrameofReferenceUID || !StudyInstanceUID) throw std::runtime_error("Missing needed image metadata.");

                        const std::string save_roi = Detox_String(Execute_Command_In_Pipe("zenity --question --text='Save ROI?' 2>/dev/null && echo 1"));
                        if(save_roi != "1"){
                            FUNCINFO("Not saving contours. Here it is for inspection purposes:" << contour_coll_shtl.write_to_string());
                            throw std::runtime_error("Instructed not to save.");
                        }

                        const std::string roi_name = Detox_String(Execute_Command_In_Pipe(
                            "zenity --entry --text='What is the name of the ROI?' --entry-text='ICCR2016_' 2>/dev/null"));
                        if(roi_name.empty()) throw std::runtime_error("Cannot save with an empty ROI name. (Punctuation is removed.)");

                        //Trim empty contours from the shuttle.
                        contour_coll_shtl.Purge_Contours_Below_Point_Count_Threshold(3);
                        if(contour_coll_shtl.contours.empty()) throw std::runtime_error("Given empty contour collection. Contours need >3 points each.");
                        const std::string cc_as_str( contour_coll_shtl.write_to_string() );                             

                        //Attempt to save to a database.
                        const std::string db_params("dbname=pacs user=hal host=localhost port=5432");
                        pqxx::connection c(db_params);
                        pqxx::work txn(c);

                        std::stringstream ss;
                        ss << "INSERT INTO contours ";
                        ss << "    (ROIName, ContourCollectionString, StudyInstanceUID, FrameofReferenceUID) ";
                        ss << "VALUES ";
                        ss << "    (" << txn.quote(roi_name);
                        ss << "    ," << txn.quote(cc_as_str);
                        ss << "    ," << txn.quote(StudyInstanceUID.value());
                        ss << "    ," << txn.quote(FrameofReferenceUID.value());
                        ss << "    ) ";   // TODO: Insert contour_collection's metadata?
                        ss << "RETURNING ROIName;";

                        FUNCINFO("Executing query:\n\t" << ss.str());
                        pqxx::result res = txn.exec(ss.str());
                        if(res.empty()) throw std::runtime_error("Should have received an ROIName but didn't.");
                        txn.commit();

                        //Clear the data in preparation for the next contour collection.
                        contour_coll_shtl.contours.clear();
                        contour_coll_shtl.contours.emplace_back();
                        contour_coll_shtl.contours.back().closed = true;

                        FUNCINFO("Contour collection saved to db and cleared");
                    }catch(const std::exception &e){
                        FUNCWARN("Unable to push contour collection to db: '" << e.what() << "'");
                    }

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
                            custom_width.swap( img_win_fw );
                            custom_centre.swap( img_win_c );
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
                    sf::Vector2f ClickWorldPos = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x,event.mouseButton.y));
                    sf::FloatRect DispImgBBox = disp_img_texture_sprite.second.getGlobalBounds();
                    if(DispImgBBox.contains(ClickWorldPos)){
                        // ---- Draw on the image where we have clicked ----
                        //Assuming the image is not rotated or skewed (though possibly scaled), determine which image pixel
                        // we are hovering over.
                        const auto clamped_col_as_f = fabs(ClickWorldPos.x - DispImgBBox.left)/(DispImgBBox.width);
                        const auto clamped_row_as_f = fabs(DispImgBBox.top - ClickWorldPos.y )/(DispImgBBox.height);

                        const auto img_w_h = disp_img_texture_sprite.first.getSize();
                        const auto col_as_u = static_cast<uint32_t>(clamped_col_as_f * static_cast<float>(img_w_h.x));
                        const auto row_as_u = static_cast<uint32_t>(clamped_row_as_f * static_cast<float>(img_w_h.y));

                        //FUNCINFO("Suspected updated row, col = " << row_as_u << ", " << col_as_u);                           
                        sf::Uint8 newpixvals[4] = {255, 0, 0, 255};
                        disp_img_texture_sprite.first.update(newpixvals,1,1,col_as_u,row_as_u);
                        disp_img_texture_sprite.second.setTexture(disp_img_texture_sprite.first);

                        const auto dicom_pos = disp_img_it->position(row_as_u, col_as_u); 
                        auto FrameofReferenceUID = disp_img_it->GetMetadataValueAs<std::string>("FrameofReferenceUID");
                        if(FrameofReferenceUID){
                            //Record the point in the working contour buffer.
                            contour_coll_shtl.contours.back().closed = true;
                            contour_coll_shtl.contours.back().points.push_back( dicom_pos );
                            contour_coll_shtl.contours.back().metadata["FrameofReferenceUID"] = FrameofReferenceUID.value();
                        }else{
                            FUNCWARN("Unable to find display image's FrameofReferenceUID. Cannot insert point in contour");

                        }
                    }

                }

            }else if(window.hasFocus() && (event.type == sf::Event::MouseButtonReleased)){

            }else if(window.hasFocus() && (event.type == sf::Event::MouseMoved)){
                cursortext.setPosition(event.mouseMove.x,event.mouseMove.y);
                smallcirc.setPosition(event.mouseMove.x - smallcirc.getRadius(),
                                      event.mouseMove.y - smallcirc.getRadius());

                //Print the world coordinates to the console.
                sf::Vector2f worldPos = window.mapPixelToCoords(sf::Vector2i(event.mouseMove.x,event.mouseMove.y));

                //Display info at the cursor about which image pixel we are on, pixel intensity.
                sf::Vector2f ClickWorldPos = window.mapPixelToCoords(sf::Vector2i(event.mouseMove.x,event.mouseMove.y));
                sf::FloatRect DispImgBBox = disp_img_texture_sprite.second.getGlobalBounds();
                if(DispImgBBox.contains(ClickWorldPos)){
                    //Assuming the image is not rotated or skewed (though possibly scaled), determine which image pixel
                    // we are hovering over.
                    const auto clamped_col_as_f = fabs(ClickWorldPos.x - DispImgBBox.left)/(DispImgBBox.width);
                    const auto clamped_row_as_f = fabs(DispImgBBox.top - ClickWorldPos.y )/(DispImgBBox.height);

                    const auto img_w_h = disp_img_texture_sprite.first.getSize();
                    const auto col_as_u = static_cast<long int>(clamped_col_as_f * static_cast<float>(img_w_h.x));
                    const auto row_as_u = static_cast<long int>(clamped_row_as_f * static_cast<float>(img_w_h.y));

                    //FUNCINFO("Suspected updated row, col = " << row_as_u << ", " << col_as_u);
                    const auto pix_val = disp_img_it->value(row_as_u,col_as_u,0);
                    std::stringstream ss;
                    ss << "(r,c)=(" << row_as_u << "," << col_as_u << ") -- " << pix_val;
                    ss << "    (DICOM Position)=" << disp_img_it->position(row_as_u,col_as_u);
                    cursortext.setString(ss.str());
                    BLcornertextss.str(""); //Clear stringstream.
                    BLcornertextss << ss.str();
                }else{
                    cursortext.setString("");
                    BLcornertextss.str(""); //Clear stringstream.
                }


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
            sf::Event event;
            while(plotwindow.pollEvent(event)){
                if(event.type == sf::Event::Closed){
                    plotwindow.close();
                }else if(plotwindow.hasFocus() && (event.type == sf::Event::KeyPressed)){
                    if(event.key.code == sf::Keyboard::Escape){
                        plotwindow.close();
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
            if(custom_centre && custom_width){
                const auto existing = BLcornertext.getString();
                std::stringstream ss;
                ss << existing.toAnsiString() << std::endl 
                   << "Custom c/w: " << custom_centre.value()
                   << " / " << custom_width.value();
                BLcornertext.setString(ss.str());
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
        window.draw(smallcirc);
        if(drawcursortext) window.draw(cursortext);
        window.draw(BLcornertext);

        //Draw any contours that lie in the plane of the current image. Also draw contour names if the cursor is 'within' them.
        if(ShowExistingContours && (DICOM_data.contour_data != nullptr)){
            sf::Text contourtext;
            std::stringstream contourtextss;
            contourtext.setFont(afont);
            contourtext.setString("");
            contourtext.setCharacterSize(12); //Size in pixels, not in points.
            contourtext.setColor(sf::Color::Green);

            for(auto & cc : DICOM_data.contour_data->ccs){
                //auto base_ptr = reinterpret_cast<contour_collection<double> *>(&cc);
                for(auto & c : cc.contours){
                    if(disp_img_it->encompasses_contour_of_points(c)){
                        sf::VertexArray lines;
                        lines.setPrimitiveType(sf::LinesStrip);

                        //Change colour depending on the orientation.
                        const auto arb_pos_unit = disp_img_it->row_unit.Cross(disp_img_it->col_unit).unit();
                        const auto c_orient = c.Estimate_Planar_Normal();
                        const auto c_orient_pos = (c_orient.Dot(arb_pos_unit) > 0);
                        const auto c_color = ( c_orient_pos ? sf::Color::Red : sf::Color::Blue );

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
                if(!c.points.empty() && disp_img_it->encompasses_contour_of_points(c)){
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

                        lines.append( sf::Vertex( sf::Vector2f(world_x, world_y), sf::Color::Blue ) );
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
            if(!window.capture().saveToFile(fname_sshot)){
                FUNCWARN("Unable to dump screenshot to file '" << fname_sshot << "'");
            }
        }

        // -------------------------------- Plotting Window Rendering ----------------------------------------

        do{
            if(!plotwindow.isOpen()) break;
            if(!window.hasFocus()) break;
        
            plotwindow.clear(sf::Color::Black);

            // ---- Get time course data for the current pixel, if available ----

            //Get the *realtime* current mouse coordinates.
            const sf::Vector2i curr_m_pos = sf::Mouse::getPosition(window);

            //Convert to SFML/OpenGL "World" coordinates. 
            sf::Vector2f curr_m_pos_w = window.mapPixelToCoords(curr_m_pos);

            //Check if the mouse is within the image bounding box. We can only proceed if it is.
            sf::FloatRect disp_img_bbox = disp_img_texture_sprite.second.getGlobalBounds();
            if(!disp_img_bbox.contains(curr_m_pos_w)){
                //FUNCWARN("Mouse not over the image. Cannot dump time course");
                break;
            }

            //Assuming the image is not rotated or skewed (though possibly scaled), determine which image pixel
            // we are hovering over.
            const auto clamped_col_as_f = fabs(curr_m_pos_w.x - disp_img_bbox.left)/disp_img_bbox.width;
            const auto clamped_row_as_f = fabs(disp_img_bbox.top - curr_m_pos_w.y )/disp_img_bbox.height;

            const auto img_w_h = disp_img_texture_sprite.first.getSize();
            const auto col_as_u = static_cast<uint32_t>(clamped_col_as_f * static_cast<float>(img_w_h.x));
            const auto row_as_u = static_cast<uint32_t>(clamped_row_as_f * static_cast<float>(img_w_h.y));
            //FUNCINFO("Dumping time course for row,col = " << row_as_u << "," << col_as_u);

            //Get the YgorImage (DICOM) pixel coordinates.
            const auto pix_pos = disp_img_it->position(row_as_u,col_as_u);

            //Get a list of images which spatially overlap this point. Order should be maintained.
            const auto ortho = disp_img_it->row_unit.Cross( disp_img_it->col_unit ).unit();
            const std::list<vec3<double>> points = { pix_pos, pix_pos + ortho * disp_img_it->pxl_dz * 0.25,
                                                              pix_pos - ortho * disp_img_it->pxl_dz * 0.25 };
            auto encompassing_images = (*img_array_ptr_it)->imagecoll.get_images_which_encompass_all_points(points);

            //Cycle over the images, dumping the ordinate (pixel values) vs abscissa (time) derived from metadata.
            samples_1D<double> shtl;
            const std::string quantity("dt"); //As it appears in the metadata. Must convert to a double!
            const bool sort_on_append = false;

            for(const auto &enc_img_it : encompassing_images){
                if(auto abscissa = enc_img_it->GetMetadataValueAs<double>(quantity)){
                    try{
                        const auto pix_val = enc_img_it->value(pix_pos,0);
                        shtl.push_back( abscissa.value(), 0.0, 
                                        static_cast<double>(pix_val), 0.0, sort_on_append );
                    }catch(const std::exception &){ }
                }
            }
            shtl.stable_sort();

            if(shtl.size() < 2) break;

            // Draw a plot.

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
                  || ( D_V < std::numeric_limits<float>::min() ) ) break; //Too small to plot...

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

                //Plot the data.
                {
                    std::vector<sf::Vertex> verts;
                    for(const auto & d : shtl.samples){
                        verts.emplace_back( sf::Vector2f( D_to_V_map_H(d[0]), D_to_V_map_V(d[2]) ), sf::Color::Yellow );
                    }
                    plotwindow.draw(verts.data(), verts.size(), sf::PrimitiveType::LineStrip);
                    
                    for(const auto & d : shtl.samples){
                        sf::CircleShape acirc;
                        const float r = 2.0f;
                        acirc.setRadius(r);
                        acirc.setFillColor(sf::Color(255,69,0,255)); // Orange.
                        acirc.setOutlineColor(sf::Color::Yellow);
                        acirc.setOutlineThickness(1.0f);
                        acirc.setPosition( sf::Vector2f( D_to_V_map_H(d[0]), D_to_V_map_V(d[2]) ) );
                        plotwindow.draw(acirc);
                    }
                }

            }

            plotwindow.display();

        }while(false);

    }

    return std::move(DICOM_data);
}
