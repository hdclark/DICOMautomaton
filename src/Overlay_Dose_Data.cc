//Overlay_Dose_Data.cc - A part of the DICOMautomaton project 2012-2013.
//
// This program is a generic program used for performing graphical tasks with data from 
// DICOM-format files. Development is ongoing, as this is a sort of testbed for ideas
// for the DICOMautomaton family.
//
// Written by hal clark.

#include <GL/freeglut_std.h>
#include <GL/gl.h>          // Header File For The OpenGL32 Library.
#include <YgorFilesDirs.h>
#include <YgorImages.h>
#include <getopt.h>         //Needed for 'getopts' argument parsing.
#include <unistd.h>         // Needed to sleep.
#include <algorithm>
#include <cmath>
#include <cstdint>          //Needed for intmax_t
#include <cstdlib>          //Needed for exit() calls.
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <random>
#include <string>    
#include <tuple>
#include <type_traits>
#include <vector>

#include "Imebra_Shim.h"   //Wrapper for Imebra library.
#include "Structs.h"
#include "YgorContainers.h"  //Needed for bimap class.
#include "YgorMath.h"        //Needed for vec3 class.
#include "YgorMisc.h"        //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"

//GLUT does NOT have compatability with scroll wheels. This is here to cope with it.
#ifndef GLUT_WHEEL_UP
    #define GLUT_WHEEL_UP (3)
#endif

#ifndef GLUT_WHEEL_DOWN
    #define GLUT_WHEEL_DOWN (4)
#endif

#define ESCAPE 27               //ASCII code for the escape key.

const std::string Version = "0.1.3 - Beta. Use at your own risk!";

int window;                     //GLUT window number.
int Screen_Pixel_Width  = 768;  //Initial screen width. Can be resized during runtime.
int Screen_Pixel_Height = 768;  //Initial screen height. Can be reseized during runtime.
int Scene_Count = 0;
int Scene_Count_Max = 100000;
long int which_frame = 0;
int vertpixcount;               //This is the number of vert pixels
int horizpixcount;              //This is the number of horiz pixels.
long int chosen_contour  = 0;   //This is a way to pick out which contour is which. This contour gets a special color.
long int chosen_subseg   = -1;  //This is a way to pick out which subsegmentation is which. This contour gets a special color.
bool printed_subseg_string = false;

float ZOOM         =  1.0; 
float Ortho_Left   = -2000.0;
float Ortho_Right  =  2000.0;
float Ortho_Top    =  2000.0;
float Ortho_Bottom = -2000.0;
float Ortho_Inner  =  5000.0;
float Ortho_Outer  = -5000.0;

float Lateral_Rot  = 0.0;
float SupInf_Rot   = 0.0;
float PostAnt_Rot  = 0.0;

long int Dose_Frame_Offset = 0;
float Dose_Tweak_Vert   =  0.0;
float Dose_Tweak_Horiz  =  0.0;

float Contour_Tweak_Vert  = 0.0;
float Contour_Tweak_Horiz = 0.0;

float CT_Tweak_Vert   =  0.0;
float CT_Tweak_Horiz  =  0.0;

float CT_Tweak_Bright   = 0.385543;    //Dimmer is better for contour viewing.
float Dose_Tweak_Bright = 0.385543; 

bool Show_Segmented_Contours = false;  //Switches showing the subsegmented contour data versus the 'regular' contour data.
bool Show_Bounding_Box = false;        //Shows a bounding box oriented along the ANT-POST direction.

bool Sleep_Mode = false;               //Inserts a sleep command within the loop to force a reduction in screen update speed. 
bool Auto_Adjust_Orthos = true;        //Attempt to adjust the ortho planes to avoid skewing images. Clamps to dose, images.

std::vector<std::string> Filenames_In;
std::string FilenameOut, FilenameLex;

std::stringstream ShuttleOut;
bool Intercept_Contour_Data = false; 
bool Dump_As_Gnuplot_Directives = false;
bool Dump_Frame_As_Image = false;

bool VERBOSE = false;

bimap<std::string,long int> Contour_classifications;
Drover DICOM_data;

std::unique_ptr<Contour_Data> Subsegmented_New__Style_Contour_Data;
std::unique_ptr<Contour_Data> Bounding_Box_Contour_Data;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ReSizeGLScene(int Width, int Height){
    if(Height==0) Height=1;
    glViewport(0,0,Width,Height);
    Screen_Pixel_Width  = Width;
    Screen_Pixel_Height = Height;
    glMatrixMode(GL_MODELVIEW);
    return;
}

void InitGL(int Width, int Height){
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(Ortho_Left*ZOOM,Ortho_Right*ZOOM,Ortho_Bottom*ZOOM,Ortho_Top*ZOOM,Ortho_Inner,Ortho_Outer);
    glMatrixMode(GL_MODELVIEW);
    //glClearColor(0.15f,0.30f,0.70f,0.3f); //Ugly, distinctive blue.
    glClearColor(0.0f,0.0f,0.0f,0.3f);
    glDisable(GL_DEPTH_TEST); //Disables Depth Testing
    glShadeModel(GL_SMOOTH);  //Enables Smooth Color Shading
    glEnable(GL_BLEND);       //Make sure to supply a fourth coordinate with each glColor4f() call.
    //glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_ONE,GL_ONE);
    //glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    //glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ZERO);
    ReSizeGLScene(Width,Height);
    return;
}

void DrawGLScene(){
    if(Scene_Count == 0) InitGL(Screen_Pixel_Width, Screen_Pixel_Height);
    glClear(GL_COLOR_BUFFER_BIT); //Clear the screen/depth buffer.
    glLoadIdentity();

    //Rotate about the object. Translate into it's center, rotate the camera, and
    // translate back. This gives the illusion of the object spinning.
    if(DICOM_data.Has_Dose_Data() || DICOM_data.Has_Image_Data()){
        vec3<double> center;
        if(DICOM_data.Has_Dose_Data()) center = DICOM_data.dose_data.front()->imagecoll.center();
        if(DICOM_data.Has_Image_Data()) center = DICOM_data.image_data.front()->imagecoll.center();

        glTranslatef(center.x,center.y,center.z);
        glRotatef(Lateral_Rot,1.0,0.0,0.0);
        glRotatef( SupInf_Rot,0.0,1.0,0.0);
        glRotatef(PostAnt_Rot,0.0,0.0,1.0);
        glTranslatef(-center.x,-center.y,-center.z);
    }

    //Draw some orientation reference axes (Note: these are *not* at the DICOM origin!)
    // The terminus of the lines will show the ortho box (dose box, ct edges, etc..)
    glBegin(GL_LINES);
    glColor4f(1.00,1.00,1.00,1.00); //X-axis: white.

    glVertex3f( Ortho_Left,0.0,-0.5*(Ortho_Inner+Ortho_Outer));
    glVertex3f(Ortho_Right,0.0,-0.5*(Ortho_Inner+Ortho_Outer));

    glColor4f(1.00,0.08,0.75,1.00); //Y-axis: pink.
    glVertex3f(0.0,Ortho_Bottom,-0.5*(Ortho_Inner+Ortho_Outer));
    glVertex3f(0.0,   Ortho_Top,-0.5*(Ortho_Inner+Ortho_Outer));

    glColor4f(1.00,0.75,0.00,1.00); //Z-axis: orange.
    glVertex3f(0.0,0.0,Ortho_Inner);
    glVertex3f(0.0,0.0,Ortho_Outer);
    glEnd();

    //Keep track of the last dose planar image displayed. We use it to adjust the orthos, more
    // quickly scan through contour data, etc.. Always check it is non-nullptr.
    planar_image<float, double> *dose_image_displayed = nullptr;

    //------------------------------------------ Dose Data ----------------------------------------------
    if(DICOM_data.Has_Dose_Data()){
        //Cycle over the dose information from the different dose files.
        //
        //NOTE: We currently do NOT sum the doses in each voxel and rather just draw sequentially.
        // This should be fixed (or proper alpha support should be provided.)
        glBegin(GL_QUADS);
        for(auto & l_it : DICOM_data.dose_data){
            for(auto pi_it = l_it->imagecoll.images.begin(); pi_it != l_it->imagecoll.images.end(); ++pi_it){
                //Check if this image has the frame number we desire. If not, skip drawing it.
                if(which_frame < static_cast<long int>(l_it->imagecoll.images.size())){
                    if(static_cast<long int>(std::distance(l_it->imagecoll.images.begin(),pi_it)) != which_frame) continue;
                    dose_image_displayed = &(*pi_it);
                }
   
                //Cycle over pixels.
                float R = 0.0, G = 0.0, B = 0.0;
                for(long int r = 0; r < pi_it->rows; ++r) for(long int c = 0; c < pi_it->columns; ++c){
                    const auto val = pi_it->value(r,c,0); //Red channel only.

                    //Clamp/scale the dose to something reasonable. We can scale it while viewing. 50Gy works.
                    R = Dose_Tweak_Bright * static_cast<double>(val)*l_it.get()->grid_scale/50.0;
                    //G = ...
                    //B = ...
                    if(R > 1.0) R = 1.0;
                    glColor4f(R,G,B, 0.7);
        
                    vec3<double> pos = pi_it->position(r,c);
                    pos.x += Dose_Tweak_Horiz;
                    pos.y += Dose_Tweak_Vert;

                    glVertex3f( (pos.x - 0.5*pi_it->pxl_dx), (pos.y + 0.5*pi_it->pxl_dy), pos.z );
                    glVertex3f( (pos.x - 0.5*pi_it->pxl_dx), (pos.y - 0.5*pi_it->pxl_dy), pos.z );
                    glVertex3f( (pos.x + 0.5*pi_it->pxl_dx), (pos.y - 0.5*pi_it->pxl_dy), pos.z );
                    glVertex3f( (pos.x + 0.5*pi_it->pxl_dx), (pos.y + 0.5*pi_it->pxl_dy), pos.z );
                }
            }
        }
        glEnd();
    }

    //------------------------------------------ Image Data ----------------------------------------------
    if(DICOM_data.Has_Image_Data() && (which_frame >= 0)){
        //Cycle over image data.
        //

        glBegin(GL_QUADS);

        long int frame_number = 0;
        for(auto & l_it : DICOM_data.image_data){
            for(auto pi_it = l_it->imagecoll.images.begin(); pi_it != l_it->imagecoll.images.end(); ++pi_it){
                ++frame_number;
                if((frame_number - 1) != which_frame){
                    continue;
                }

                //Used to, for example, rescale the images to fill the screen.
                if(dose_image_displayed == nullptr) dose_image_displayed = &(*pi_it);
       
                //Cycle over pixels.
                float R = 0.0, G = 0.0, B = 0.0;
                for(long int r = 0; r < pi_it->rows; ++r) for(long int c = 0; c < pi_it->columns; ++c){
                    const auto val = pi_it->value(r,c,0); //Red channel only.

                    //Clamp/scale the dose to something reasonable. We can scale it while viewing.
                    // Lack of windowing is a serious omission!
                    G = CT_Tweak_Bright * 0.5 * (static_cast<double>(val) + 1000.0)/1000.0;
                    if(G > 1.0) G = 1.0;
                    if(G < 0.0) G = 0.0;
                    B = G;
                    R = G;
        
                    vec3<double> pos = pi_it->position(r,c);
                    pos.x += CT_Tweak_Horiz;
                    pos.y += CT_Tweak_Vert;

                    //Only draw if you need to. Do not draw completely black things.
                    if((R != 0.0) && (G != 0.0) && (B != 0.0)){
                        glColor4f(R,G,B, 0.7);
                        glVertex3f( (pos.x - 0.5*pi_it->pxl_dx), (pos.y + 0.5*pi_it->pxl_dy), pos.z );
                        glVertex3f( (pos.x - 0.5*pi_it->pxl_dx), (pos.y - 0.5*pi_it->pxl_dy), pos.z );
                        glVertex3f( (pos.x + 0.5*pi_it->pxl_dx), (pos.y - 0.5*pi_it->pxl_dy), pos.z );
                        glVertex3f( (pos.x + 0.5*pi_it->pxl_dx), (pos.y + 0.5*pi_it->pxl_dy), pos.z );
                    }
                }
            }
        }
        glEnd();
    }

    //----------------------------------------- Contour Data --------------------------------------------
    if(DICOM_data.Has_Contour_Data()){

        //Switch to segmented data if the user wants (and it exists).
        decltype(DICOM_data.contour_data.get()) Which_contour_data;
        if(Show_Segmented_Contours && (Subsegmented_New__Style_Contour_Data != nullptr)){
            Which_contour_data = Subsegmented_New__Style_Contour_Data.get();
        }else{
            Which_contour_data = DICOM_data.contour_data.get();
        }

        long int dummy_sep = 0; //Used to more easily distinguish contour families (segmentations, etc).
        long int dummy_contour_sep = 0; //Used to distinguish individual contours.
        for(auto cc_it = Which_contour_data->ccs.begin(); cc_it != Which_contour_data->ccs.end(); ++cc_it){
            ++dummy_sep;
            for(auto c_it = cc_it->contours.begin(); c_it != cc_it->contours.end(); ++c_it){
                //Check if contour is within the inner and outer orthos! If it is outside, don't draw it.
 
                // ...

                //If there are (somehow) too few points in the contour, do not attempt to draw it.
                if(c_it->points.size() < 3) continue;

                //If we have dose, check if this contour is within the thickness of the image. If not, omit it.
                if(dose_image_displayed != nullptr){
                    const auto avg_point = c_it->First_N_Point_Avg(3); //Centroid(); //Average_Point();
                    if(!dose_image_displayed->sandwiches_point_within_top_bottom_planes(avg_point)) continue;
                }

                //Check if there are a sufficient number of points in the contour (sometimes due to incomplete 
                // erasing there are very small bits of contours lying around).
                if(c_it->points.size() < 3) continue;
                glBegin(GL_LINE_STRIP); //GL_LINE_LOOP is occasionally buggy. We simply link the ends ourselves.

                //If the contour is of special interest, print it in a different colour.
                glColor4f(0.0f, 0.0f, 1.0f, 0.2f);
                if(cc_it->ROI_number == chosen_contour) glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                if(dummy_sep == chosen_subseg){
                    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);

                    //If we haven't dumped the subseg string then do it and flip the indicator.
                    if(!printed_subseg_string){
                        printed_subseg_string = true;
                        FUNCINFO("Highlighted subseg: '" << Segmentations_to_Words(cc_it->Segmentation_History) << "'");
                    }
                }

                if((Intercept_Contour_Data == true) && (cc_it->ROI_number == chosen_contour)){
                    if(Dump_As_Gnuplot_Directives){
                        ++dummy_contour_sep;

                        ShuttleOut << "set object " << dummy_contour_sep << " polygon from "; //Note: this id is also used below.
                    }
                }

                if(c_it->closed){
                    auto p_it = --(c_it->points.end());
                    float central_x = static_cast<float>(p_it->x) + Contour_Tweak_Horiz;
                    float central_y = static_cast<float>(p_it->y) + Contour_Tweak_Vert;
                    float central_z = static_cast<float>(p_it->z);
                    glVertex3f(central_x,central_y,central_z);

                    if((Intercept_Contour_Data == true) && (cc_it->ROI_number == chosen_contour)){
                        if(Dump_As_Gnuplot_Directives){
                            //Directly-Gnuplot parseable/loadable polygon objects.
                            ShuttleOut << "(DX+" << central_x << "),(DY+" << central_y << "),(DZ+" << central_z << ") ";
                        }else{
                            //Plain-jane x1 y1 z1 x2 y2 z2 raw data.
                            ShuttleOut << central_x << " " << central_y << " " << central_z << " ";  //Do not terminate.
                        }
                    }
                }

                for(auto p_it = c_it->points.begin(); p_it != c_it->points.end(); ++p_it){
                    float central_x = static_cast<float>(p_it->x) + Contour_Tweak_Horiz;   
                    float central_y = static_cast<float>(p_it->y) + Contour_Tweak_Vert;
                    float central_z = static_cast<float>(p_it->z);
                    glVertex3f(central_x,central_y,central_z);

                    if((Intercept_Contour_Data == true) && (cc_it->ROI_number == chosen_contour)){ 
                        if(Dump_As_Gnuplot_Directives){
                            //Directly-Gnuplot parseable/loadable polygon objects.
                            ShuttleOut << " to (DX+" << central_x << "),(DY+" << central_y << "),(DZ+" << central_z << ") ";

                        }else{
                            //Plain-jane x1 y1 z1 x2 y2 z2 raw data.
                            ShuttleOut << central_x << " " << central_y << " " << central_z << " " << dummy_sep << std::endl; //Terminate line.
                            if(std::next(p_it) != c_it->points.end()){
                                ShuttleOut << central_x << " " << central_y << " " << central_z << " ";
                            }
                        }
                    }
                }

                if((Intercept_Contour_Data == true) && (cc_it->ROI_number == chosen_contour)){
                    if(Dump_As_Gnuplot_Directives){
                        //Append some basic things, such as color and alpha.
                        ShuttleOut << " ; ";
                        //ShuttleOut << "set object " << dummy_contour_sep << " fc rgb '#FF0000' fillstyle solid 0.5 border lt 6 ;";
                        //ShuttleOut << "set object " << dummy_contour_sep << " fc rgb '#FF0000' fillstyle transparent solid 0.5 border lt 6 ;";
                        ShuttleOut << "set object " << dummy_contour_sep << " fc rgb '#FF0000' fillstyle transparent solid 0.5 border lc rgb '#000000' ;";

                        //Optionally tack on some additional info at the end of the line so we can grep contours if needed.
                        ShuttleOut << " # contourfamily='" << dummy_sep << "' ";

                        //Finish the line.
                        ShuttleOut << std::endl;
                    }
                }

                glEnd();
            }
        }
    }

    if(Dump_Frame_As_Image){
        Dump_Frame_As_Image = false;
        const long int num_of_bytes = 3*Screen_Pixel_Width*Screen_Pixel_Height; //Using GL_RGB and GL_UNSIGNED_BYTE.
        const auto suffix = "."_s + Xtostring(Screen_Pixel_Width) + "x"_s + Xtostring(Screen_Pixel_Height) + ".u8rgb.raw"_s;
        const std::string FNO = Get_Unique_Sequential_Filename("/tmp/Overlaydosedata_frame_-_", 4, suffix);
        std::unique_ptr<uint8_t[]> pxlbuf( new uint8_t[num_of_bytes] );

        //If this happens before the buffers swap, use GL_BACK. Otherwise, GL_FRONT.
        glReadBuffer(GL_BACK);

        //These need to be set to avoid getting a skewed image due to pixel packing options. See 
        // https://groups.google.com/forum/#!topic/comp.graphics.api.opengl/_ZfAYieHx_o and
        // http://www.opengl.org/wiki/GLAPI/glPixelStore . 
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);    //Default: 0
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);   //Default: 0
        glPixelStorei(GL_PACK_ALIGNMENT, 1);     //Default: 4
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   //Default: 4

        glReadPixels(0,0,Screen_Pixel_Width,Screen_Pixel_Height,GL_RGB,GL_UNSIGNED_BYTE,pxlbuf.get());

        //Write the raw data to file.
        if(!Write_Binary_File(FNO, std::move(pxlbuf), static_cast<intmax_t>(num_of_bytes))){
            FUNCWARN("Unable to write frame to raw file");
        }else{
            FUNCINFO("Wrote frame to file '" << FNO << "'");
            FUNCINFO("To convert:   convert -size " << Screen_Pixel_Width << "x" << Screen_Pixel_Height << " -depth 8 'rgb:" << FNO << "' out.png");
        }

        //Reset pixelstore values altered to defaults.
        //glPixelStorei(GL_PACK_ROW_LENGTH, 0);    //Default: 0
        //glPixelStorei(GL_PACK_SKIP_PIXELS, 0);   //Default: 0
        glPixelStorei(GL_PACK_ALIGNMENT, 4);       //Default: 4
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);     //Default: 4
    }

    //Replace the draw buffer with the screen buffer.
    glutSwapBuffers();

    //If we have displayed some dose, attempt to fix the orthos based on it.
    if(Auto_Adjust_Orthos && (dose_image_displayed != nullptr)){
        Auto_Adjust_Orthos = false;

        //Dose slice dimensions.
        const auto topleft = dose_image_displayed->position(0,0);
        const auto btmrght = dose_image_displayed->position(dose_image_displayed->rows-1,dose_image_displayed->columns-1);
        const auto center  = (topleft + btmrght)/2.0;
        const auto width   = 1.3*YGORABS(btmrght.x - topleft.x);  //The sign depends on the orientation unit vectors.
        const auto height  = 1.3*YGORABS(topleft.y - btmrght.y);

        //Screen dimensions.
        const auto swidth  = static_cast<double>(Screen_Pixel_Width);
        const auto sheight = static_cast<double>(Screen_Pixel_Height);
        const auto saspect = swidth/sheight;

        //Try make the top/bottom edges flush on the window edge. Else, the left/right edges.
        Ortho_Left   = center.x - 0.5*( (height*saspect > width) ? height*saspect : width );
        Ortho_Right  = center.x + 0.5*( (height*saspect > width) ? height*saspect : width );
        Ortho_Top    = center.y + 0.5*( (height*saspect > width) ? height : width/saspect );
        Ortho_Bottom = center.y - 0.5*( (height*saspect > width) ? height : width/saspect );

        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);
    }

    ++Scene_Count;
    if(Scene_Count > Scene_Count_Max) FUNCERR("Scene update limit achieved. Goodbye");


    //Reset/re-prepare if we have just finished a data dump.
    if(Intercept_Contour_Data == true){
        if(!WriteStringToFile(ShuttleOut.str(), FilenameOut)){
            FUNCWARN("Unable to write data to file. Continuing");
        }else{
            FUNCINFO("Wrote raw contour data to '" << FilenameOut << "'");
        }
 
        FilenameOut = Get_Unique_Filename(std::string("/tmp/DICOMautomaton_overlaydosedata_out_-_"), 10);
        ShuttleOut.clear();
        Intercept_Contour_Data = false;
        Dump_As_Gnuplot_Directives = false;
    }

    if(Sleep_Mode == true) usleep(1000000);
    return;
}

void keyPressed(unsigned char key, int /* x */, int /* y */){
    usleep(100);

    //Generic flow-control things.
    if(key == ESCAPE){
        glutDestroyWindow(window);
        exit(0);
    }else if(key == 'q'){ 
        glutDestroyWindow(window);
        exit(0);

    }else if(key == 'l'){  //Toggles sleep mode (slow update speed by sleeping the cpu each update)
        if( Sleep_Mode == true ){
            FUNCINFO("Exiting sleep mode");
            Sleep_Mode = false;
        }else{
            FUNCINFO("Entering sleep mode");
            Sleep_Mode = true;
        }

    //Graphics control - refresh the aspect calculations by adjusting ortho planes.
    }else if(key == 'r'){
        ReSizeGLScene(Screen_Pixel_Width, Screen_Pixel_Height);
        Auto_Adjust_Orthos = true;

    //Cycle through frames and adjust Ortho clipping planes.
    }else if((key == '+') || (key == '=')){  //Why? Silly keyboard layout for '+' character.
        ++which_frame;
        FUNCINFO("Viewing frame " << which_frame);
    }else if(key == '-'){
        --which_frame;
        FUNCINFO("Viewing frame " << which_frame);

    //ZOOM.
    }else if(key == 'z'){   //Zoom in.
        ZOOM += 0.104351268457;
        ZOOM = fabs(ZOOM);
        FUNCINFO("Zoom is now: " << ZOOM );
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);
    }else if(key == 'Z'){   //Zoom out.
        ZOOM -= 0.107356832145;
        ZOOM = fabs(ZOOM);
        FUNCINFO("Zoom is now: " << ZOOM );
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);

    //TOP BORDER.
    }else if(key == 'w'){   //Adjust top cutoff upward.
        Ortho_Top += 0.25*(Ortho_Top - Ortho_Bottom);//100.123456789;
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);
    }else if(key == 'W'){   //Adjust top cutoff downward.
        Ortho_Top -= 0.25*(Ortho_Top - Ortho_Bottom);//100.218765498;
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);

    //BOTTOM BORDER.
    }else if(key == 's'){   //Adjust bottom cutoff upward.
        Ortho_Bottom -= 0.25*(Ortho_Top - Ortho_Bottom);//100.123456789;
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);
    }else if(key == 'S'){   //Adjust bottom cutoff downward.
        Ortho_Bottom += 0.25*(Ortho_Top - Ortho_Bottom);//100.218765498;
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);

    //LEFT BORDER.
    }else if(key == 'A'){   //Adjust left cutoff upward.
        Ortho_Left += 0.25*(Ortho_Right - Ortho_Left);//100.123456789;
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);
    }else if(key == 'a'){   //Adjust left cutoff downward.
        Ortho_Left -= 0.25*(Ortho_Right - Ortho_Left);//100.218765498;
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);

    //RIGHT BORDER.
    }else if(key == 'D'){   //Adjust right cutoff upward.
        Ortho_Right -= 0.25*(Ortho_Right - Ortho_Left);//100.123456789;
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);
    }else if(key == 'd'){   //Adjust right cutoff downward.
        Ortho_Right += 0.25*(Ortho_Right - Ortho_Left);//100.218765498;
        InitGL(Screen_Pixel_Width, Screen_Pixel_Height);

    //DOSE TWEAKING.
    }else if(key == 'v'){   //Adjust upward.
        Dose_Tweak_Vert += 1.0;
        FUNCINFO("Dose_Tweak vertical parameter is " << Dose_Tweak_Vert );
    }else if(key == 'f'){   //Adjust downward.
        Dose_Tweak_Vert -= 1.0;
        FUNCINFO("Dose_Tweak vertical parameter is " << Dose_Tweak_Vert );
    }else if(key == 'b'){   //Adjust leftward.
        Dose_Tweak_Horiz += 1.0;
        FUNCINFO("Dose_Tweak horizontal parameter is " << Dose_Tweak_Horiz );
    }else if(key == 'c'){   //Adjust rightward.
        Dose_Tweak_Horiz -= 1.0;
        FUNCINFO("Dose_Tweak horizontal parameter is " << Dose_Tweak_Horiz );

    }else if(key == '7'){  //Try moving frames up/down (to try match contours, etc..)
        --Dose_Frame_Offset;
        FUNCINFO("Dose_Frame_Offset parameter is " << Dose_Frame_Offset);
    }else if(key == '&'){
        ++Dose_Frame_Offset;
        FUNCINFO("Dose_Frame_Offset parameter is " << Dose_Frame_Offset);
    
    //CONTOUR TWEAKING.
    }else if(key == 'e'){   //Toggle showing the "Exploded" (subsegmented) contours instead of the vanilla ones.
        if(!Show_Segmented_Contours){
            FUNCINFO("Showing sub-segmented ('exploded') contours");
        }else{
            FUNCINFO("Showing original contours");
        }

        Show_Segmented_Contours = (Show_Segmented_Contours == true) ? false : true ;

    }else if(key == 'x'){   //Toggle showing the bounding boxes of the contours. 
        Show_Bounding_Box = (Show_Bounding_Box == true) ? false : true ;

    }else if(key == 'n'){   //Cycle through the contours ("next")
        chosen_contour = Contour_classifications.get_next( chosen_contour );
        FUNCINFO("Highlighted contour is called \"" << Contour_classifications[chosen_contour] << "\" and has ROI tag number " << chosen_contour );

    }else if(key == 'p'){   //Cycle through the contours ("previous")
        chosen_contour = Contour_classifications.get_previous( chosen_contour );
        FUNCINFO("Highlighted contour is called \"" << Contour_classifications[chosen_contour] << "\" and has ROI tag number " << chosen_contour );

    }else if(key == 'N'){   //Cycle through the subsegmentations ("Next")
        ++chosen_subseg;
        printed_subseg_string = false;
        FUNCINFO("Highlighted subsegmentation is (arb. family number) " << chosen_subseg << ". Readable subseg string will be dumped if it exists.");

    }else if(key == 'P'){   //Cycle through the subsegmentations ("Previous")
        --chosen_subseg;
        printed_subseg_string = false;
        FUNCINFO("Highlighted subsegmentation is (arb. family number) " << chosen_subseg << ". Readable subseg string will be dumped if it exists.");

    //CT TWEAKING.
    }else if(key == 'V'){   //Adjust upward.
        CT_Tweak_Vert += 1.0;
        FUNCINFO("Tweak vertical parameter is " << CT_Tweak_Vert );
    }else if(key == 'F'){   //Adjust downward.
        CT_Tweak_Vert -= 1.0;
        FUNCINFO("Tweak vertical parameter is " << CT_Tweak_Vert );
    }else if(key == 'B'){   //Adjust leftward.
        CT_Tweak_Horiz += 1.0;
        FUNCINFO("Tweak horizontal parameter is " << CT_Tweak_Horiz );
    }else if(key == 'C'){   //Adjust rightward.
        CT_Tweak_Horiz -= 1.0;
        FUNCINFO("Tweak horizontal parameter is " << CT_Tweak_Horiz );

    //Brightness scaling. (This will cause saturation in regions of high intensity!)
    }else if(key == '['){   //Adjust darker - CT data.
        CT_Tweak_Bright /= 1.1;
        FUNCINFO("Tweak CT brightness parameter is " << CT_Tweak_Bright );
    }else if(key == ']'){   //Adjust brighter - CT data.
        CT_Tweak_Bright *= 1.1;
        FUNCINFO("Tweak CT brightness parameter is " << CT_Tweak_Bright );
    }else if(key == '{'){   //Adjust darker - Dose data.
        Dose_Tweak_Bright /= 1.1;
        FUNCINFO("Tweak dose brightness parameter is " << Dose_Tweak_Bright );
    }else if(key == '}'){   //Adjust brighter - Dose data.
        Dose_Tweak_Bright *= 1.1;
        FUNCINFO("Tweak dose brightness parameter is " << Dose_Tweak_Bright );


    //Rotation of the data. Units must be degrees.
    }else if(key == '8'){
        Lateral_Rot -= 2.5;
    }else if(key == '*'){
        Lateral_Rot += 2.5;

    }else if(key == '9'){
        PostAnt_Rot += 2.5;
    }else if(key == '('){
        PostAnt_Rot -= 2.5;

    }else if(key == '0'){
        SupInf_Rot += 2.5;
    }else if(key == ')'){
        SupInf_Rot -= 2.5;

    //On-demand sub-segmenting.
    }else if(key == '1'){
        if(Subsegmented_New__Style_Contour_Data == nullptr) return;
        FUNCINFO("Sub-segmenting per volume along coronal plane");
        Subsegmented_New__Style_Contour_Data = Subsegmented_New__Style_Contour_Data->Split_Per_Volume_Along_Coronal_Plane();

    }else if(key == '!'){
        if(Subsegmented_New__Style_Contour_Data == nullptr) return;
        FUNCINFO("Sub-segmenting per volume along sagittal plane");
        Subsegmented_New__Style_Contour_Data = Subsegmented_New__Style_Contour_Data->Split_Per_Volume_Along_Sagittal_Plane();

    }else if(key == '2'){
        if(Subsegmented_New__Style_Contour_Data == nullptr) return;
        FUNCINFO("Sub-segmenting per contour along coronal plane");
        Subsegmented_New__Style_Contour_Data = Subsegmented_New__Style_Contour_Data->Split_Per_Contour_Along_Coronal_Plane();

    }else if(key == '@'){
        if(Subsegmented_New__Style_Contour_Data == nullptr) return;
        FUNCINFO("Sub-segmenting per contour along sagittal plane");
        Subsegmented_New__Style_Contour_Data = Subsegmented_New__Style_Contour_Data->Split_Per_Contour_Along_Sagittal_Plane();

    }else if(key == '3'){
        FUNCINFO("...raycast...pervolume...antpost... - not yet implemented. Doing random split instead.");
       
        FUNCINFO("Doing a random:  Sub-segmenting per contour  instead"); 
        std::random_device rd;
        std::mt19937 random_engine( rd() );
        std::uniform_real_distribution<> random_distribution(-1.0, 1.0);
        vec3<double> R_rand( random_distribution(random_engine), random_distribution(random_engine), 0.0 );
        R_rand = R_rand.unit();
        if(random_distribution(random_engine) > 1.0) R_rand.x *= -1.0;
        if(random_distribution(random_engine) > 1.0) R_rand.y *= -1.0;
        Subsegmented_New__Style_Contour_Data = Subsegmented_New__Style_Contour_Data->Split_Per_Contour_Along_Given_Plane_Unit_Normal(R_rand);

    }else if(key == '#'){
        FUNCINFO("...raycast...pervolume...lateral... - not yet implemented - just reserved!");

    }else if(key == '4'){
        if(Subsegmented_New__Style_Contour_Data == nullptr) return;
        FUNCINFO("Sub-segmenting raycast per contour along ant-post");
        Subsegmented_New__Style_Contour_Data = Subsegmented_New__Style_Contour_Data->Raycast_Split_Per_Contour_Into_ANT_POST();

    }else if(key == '$'){
        if(Subsegmented_New__Style_Contour_Data == nullptr) return;
        FUNCINFO("Sub-segmenting raycast per contour along lateral");
        Subsegmented_New__Style_Contour_Data = Subsegmented_New__Style_Contour_Data->Raycast_Split_Per_Contour_Into_Lateral();

    }else if(key == '5'){
        if(Subsegmented_New__Style_Contour_Data == nullptr) return;
        FUNCINFO("Sub-segmenting core-peel with factor 0.85");
        Subsegmented_New__Style_Contour_Data = Subsegmented_New__Style_Contour_Data->Split_Core_and_Peel(0.85);


    //Data dumping/interception.
    }else if(key == 'i'){
        //Dumps the selected contour (or all, if all are in view) as vertex data suitable for plotting with Gnuplot.
        Intercept_Contour_Data = true;
        Dump_As_Gnuplot_Directives = false;

        //Data file header.
        ShuttleOut << "# Contour line segments generated by OverlayDoseData for contour '";
        ShuttleOut << Contour_classifications[chosen_contour]  << "' from files:" << std::endl;
        for(auto & it : Filenames_In) ShuttleOut << "# " << it << std::endl;
        ShuttleOut << "#" << std::endl << "# To plot using Gnuplot, try something like: " << std::endl;
        ShuttleOut << "# plot '" << FilenameOut << "' u 1:2:($4-$1):($5-$2) with vectors nohead " << std::endl;
        ShuttleOut << "# or, to plot all families (segments) " << std::endl;
        ShuttleOut << "# splot '" << FilenameOut << "' u 1:2:3:($4-$1):($5-$2):($6-$3) with vectors nohead " << std::endl;
        ShuttleOut << "# or, selective families  " << std::endl;
        ShuttleOut << "# splot '" << FilenameOut << "' u (  ($7 == 13) ? $1 : 1/0   ):2:3:($4-$1):($5-$2):($6-$3) with vectors nohead " << std::endl;
        ShuttleOut << "#" << std::endl << "# Columns:  x1 y1 z1 x2 y2 z2 contour_family_id" << std::endl;


    }else if(key == 'j'){
        //Dumps the selected contour (or all, if all are in view) as POLYGON data suitable for hacky-plotting with Gnuplot.
        //
        //NOTE: Use THIS method to most easily produce configurable, high-quality renderings with GNUplot!
        Intercept_Contour_Data = true;
        Dump_As_Gnuplot_Directives = true;

        //Data file header.
        ShuttleOut << "# Contour polygon (vertices) generated by OverlayDoseData for contour '";
        ShuttleOut << Contour_classifications[chosen_contour]  << "' from files:" << std::endl;
        for(auto & it : Filenames_In) ShuttleOut << "# " << it << std::endl;
        ShuttleOut << "#" << std::endl << "# To plot using Gnuplot, copy line-for-line into the prompt (or load this file). " << std::endl;
        ShuttleOut << "# NOTE: These objects can be selectively loaded by grepping the info at the end of the lines!" << std::endl;
        ShuttleOut << " DX=0.0 ; DY=0.0 ; DZ=0.0 ; " << std::endl;

    }else if(key == 'J'){
        //Signal that the (bitmap) frame should be copied and written as a raw file.
        Dump_Frame_As_Image = true;

    }else{
        FUNCINFO("Key is not bound to any action!");
    }

    return;
}


void processMouse(int /* button */, int /* state */, int /* x */, int /* y */){
    return;
}


int main(int argc, char* argv[]){

//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------ Option parsing -----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//These are fairly common options. Run the program with -h to see them formatted properly.

    int next_options;
    const char* const short_options    = "hvVi:l:o:"; //This is the list of short, single-letter options.
                                                      //The : denotes a value passed in with the option.
    //This is the list of long options. Columns:  Name, BOOL: takes_value?, NULL, Map to short options.
    const struct option long_options[] = { { "help",        0, nullptr, 'h' },
                                           { "version",     0, nullptr, 'V' },
                                           { "verbose",     0, nullptr, 'v' },
                                           { "in",          1, nullptr, 'i' },
                                           { "out",         1, nullptr, 'o' },
                                           { "lexicon",     1, nullptr, 'l' },
                                           { nullptr,          0, nullptr,  0  }  };

    do{
        next_options = getopt_long(argc, argv, short_options, long_options, nullptr);
        switch(next_options){
            case 'h': 
                std::cout << std::endl;
                std::cout << "-- " << argv[0] << " Command line switches: " << std::endl;
                std::cout << "----------------------------------------------------------------------------------------------------------" << std::endl;
                std::cout << "   Short              Long                 Default          Description" << std::endl;
                std::cout << "----------------------------------------------------------------------------------------------------------" << std::endl;
                std::cout << "   -h                 --help                                Display this message and exit." << std::endl;
                std::cout << "   -V                 --version                             Display program version and exit." << std::endl;
                std::cout << "   -v                 --verbose             <false>         Spit out info about what the program is doing." << std::endl;
                std::cout << "----------------------------------------------------------------------------------------------------------" << std::endl;
                std::cout << "   -i myfilename      --in myfilename       <none>          Incoming DICOM file names. (Required)" << std::endl;
                std::cout << "   filename                                 <none>          Incoming DICOM file names. (Required)" << std::endl;
                std::cout << "----------------------------------------------------------------------------------------------------------" << std::endl;
                std::cout << "   -o newfilename     --out newfilename     /tmp/<random>   Outgoing file name." << std::endl;
                std::cout << "   -l filename        --lexicon filename    <none>          Explicator lexicon file name." << std::endl;
                std::cout << std::endl;
                return 0;
                break;

            case 'V': 
                INFO("Version: " + Version);
                return 0;
                break;

            case 'v':
                INFO("Verbosity enabled");
                VERBOSE = true;
                break;

            case 'i':
                if(optarg != nullptr) Filenames_In.emplace_back(optarg);
                break;

            case 'l':
                if(optarg != nullptr) FilenameLex = optarg;
                break;

            case 'o':
                if(optarg != nullptr) FilenameOut = optarg;
                break;
        }
    }while(next_options != -1);
    //From the optind man page:
    //  "If the resulting value of optind is greater than argc, this indicates a missing option-argument,
    //         and getopt() shall return an error indication."
    for( ; optind < argc; ++optind){
        //We treat everything else as input files. this is OK (but not safe) because we will test each file's existence.
        FUNCWARN("Treating argument '" << argv[optind] << "' as an input filename");
        Filenames_In.emplace_back(argv[optind]);
    }

//---------------------------------------------------------------------------------------------------------------------
//----------------------------------------------- Filename Testing ----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//We now test the filenames to see if the input and output files exist. 
//So we do not overwrite the output file, we will exit if the output file already exists.
    if(VERBOSE) FUNCINFO("Now testing filenames"); 

    //Check that filenames were passed in. We will check if the *actual* files exist later.
    if(Filenames_In.empty()) FUNCERR("Input filenames not provided. Provide them or run '" << argv[0] << " -h'" );
    if(FilenameOut.empty()){ 
        //Find a good filename. We will notify the user what we have chosen.
        FilenameOut = Get_Unique_Filename(std::string("/tmp/DICOMautomaton_overlaydosedata_out_-_"), 10);
        FUNCWARN("No output filename was given - proceeding with '" << FilenameOut << "'");
    }
    if(FilenameLex.empty()){ 
        //Attempt to check a few common places. Can't think of a better technique atm.
        std::list<std::string> trial = { 
                "20150925_SGF_and_SGFQ_tags.lexicon",
                "Lexicons/20150925_SGF_and_SGFQ_tags.lexicon",
                "/usr/share/explicator/lexicons/20150925_20150925_SGF_and_SGFQ_tags.lexicon",
                "/usr/share/explicator/lexicons/20130319_SGF_filter_data_deciphered5.lexicon",
                "/usr/share/explicator/lexicons/20121030_SGF_filter_data_deciphered4.lexicon" };
        for(auto & it : trial) if(Does_File_Exist_And_Can_Be_Read(it)){
            FilenameLex = it;
            FUNCWARN("No lexicon provided - using file '" << FilenameLex << "' instead");
            break;
        }
        if(FilenameLex.empty()) FUNCERR("Lexicon not located. Please provide one. See '" << argv[0] << " -h' for info");
    }

    for(auto & it : Filenames_In){
        if(!Does_File_Exist_And_Can_Be_Read(it)) FUNCERR("Input file '" << it << "' does not exist");
    }
    if(!Does_File_Exist_And_Can_Be_Read(FilenameLex)) FUNCERR("Lexicon file '" << FilenameLex << "' does not exist");
    if(Does_File_Exist_And_Can_Be_Read(FilenameOut))  FUNCERR("Output file '" << FilenameOut << "' already exists");

    //The filenames are now set and the files are ready to be safely read/written.

//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------- File Sorting ------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //For each input file, we grab the modality and sort into filename vectors.
    std::vector<std::string> Filenames_In_Struct;  //RTSTRUCT modality. (RS structure file - contour (1D) data.)
    std::list<std::string> Filenames_In_CT;        //Image modalities.  (CT/MR/US data file. 2D pixel data.)
    std::list<std::string> Filenames_In_Dose;      //RTDOSE modality.   (RD dose files. 3D pixel data.)

    for(auto & it : Filenames_In){
        const auto mod = get_modality(it);

        if(mod == "RTSTRUCT"){ 
            Filenames_In_Struct.push_back(it);
        }else if(mod == "RTDOSE"){
            Filenames_In_Dose.push_back(it);

        //Attempt to treat image files generically. This might work, but probably won't.
        }else if(mod == "CT"){  
            Filenames_In_CT.push_back(it);
        }else if(mod == "OT"){  //"Other."
            Filenames_In_CT.push_back(it);
        }else if(mod == "US"){  //Ultrasound.
            Filenames_In_CT.push_back(it);
        }else if(mod == "MR"){  //MRI.
            Filenames_In_CT.push_back(it);
        }else{
            FUNCWARN("Unrecognized modality '" << mod << "' in file '" << it << "'. Ignoring file");
        }
    }

    //Now we blow away the old list to make sure we don't accidentally use it.
    Filenames_In.clear();

//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------- File Parsing / Data Loading ---------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

    //Load the pixel and contour data. This may take a long time, because all data is loaded into memory asap.
    if(!Filenames_In_Struct.empty()) Contour_classifications  = get_ROI_tags_and_numbers(Filenames_In_Struct[0]);
    if(!Filenames_In_Struct.empty()) DICOM_data.contour_data  = get_Contour_Data(Filenames_In_Struct[0]);
    if(!Filenames_In_CT.empty())     DICOM_data.image_data    = Load_Image_Arrays(Filenames_In_CT);
    if(!Filenames_In_Dose.empty())   DICOM_data.dose_data     = Load_Dose_Arrays(Filenames_In_Dose);

//---------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------- Processing -------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------



    //Split/group the contour data according to some heuristic (ie. dose >= XGy).
    if(false){
        DICOM_data = DICOM_data.Segment_Contours_Heuristically([](bnded_dose_pos_dose_tup_t tup) -> bool {
                //const auto pos  = std::get<0>(tup);
                //const auto r_dx = std::get<1>(tup);
                //const auto r_dy = std::get<2>(tup);
                const auto dose = std::get<3>(tup);
                //const auto i    = std::get<4>(tup);
                //const auto j    = std::get<5>(tup);
                if(dose > 45.0) return true; //Greater than X cGy.
                return false;
        });
    }

    //Meld the data. This gathers information from the various files and attempts to amalgamate it.
    if(!DICOM_data.Meld(VERBOSE)) FUNCERR("Unable to meld data");

    if(!DICOM_data.Has_Contour_Data()) FUNCINFO("We do not have any contour data. Is this intentional?");
    if(!DICOM_data.Has_Dose_Data())    FUNCINFO("We do not have any dose data. Is this intentional?");
    if(!DICOM_data.Has_Image_Data())   FUNCINFO("We do not have any image data. Is this intentional?");

    //Test the duplication mechanism by creating another drover. 
    Drover Another(DICOM_data);
    if(!Another.Meld(VERBOSE)) FUNCERR("Unable to meld data");

    //Perform some operations on the Contour data if it exists.
    if(DICOM_data.Has_Contour_Data()){
        //Print out a list of (the unique) ROI name/number correspondance.
        std::map<long int, bool> displayed;
        for(auto cc_it = DICOM_data.contour_data->ccs.begin(); cc_it != DICOM_data.contour_data->ccs.end(); ++cc_it ){
            //Check if it has come up before.
            if(displayed.find(cc_it->ROI_number) == displayed.end()){
                FUNCINFO("Contour collection with ROI number " << cc_it->ROI_number << " is named '" << cc_it->Raw_ROI_name << "'");
                displayed[cc_it->ROI_number] = true;
            }
        }

        //Copy the data. We can do on-demand sub-segmentation later.
        Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Duplicate();



        //Grab some segmented contours (for drawing purposes.)
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane();        
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Volume_Along_Sagittal_Plane();
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Height_Along_Coronal_Plane();
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Contour_Along_Coronal_Plane();
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Contour_Along_Coronal_Plane()->Split_Per_Contour_Along_Sagittal_Plane()->Split_Per_Contour_Along_Coronal_Plane()->Split_Per_Contour_Along_Sagittal_Plane()->Split_Per_Contour_Along_Coronal_Plane();

        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Volume_Along_Coronal_Plane();

        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Volume_Along_Transverse_Plane();

        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Volume_Along_Transverse_Plane()->Split_Per_Volume_Along_Coronal_Plane();

        //Segment the contours and then pick out some interesting portions.
       
        //Grab all contours which were lastly split into a 'front' part.
        //const uint32_t criteria = ( Segmentations::front );
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Contour_Along_Sagittal_Plane()->Split_Per_Volume_Along_Coronal_Plane()->Get_Contours_With_Last_Segmentation(criteria);

        //Grab all contours which have (in any order) been split along a sagittal plane and volume-split along a coronal plane.
        //const std::set<uint32_t> criteria2 = { Segmentations::sagittal | Segmentations::left,
        //                                       Segmentations::coronal | Segmentations::volume | Segmentations::front    };
        //Subsegmented_New__Style_Contour_Data = DICOM_data.contour_data->Split_Per_Volume_Along_Coronal_Plane()->Split_Per_Contour_Along_Sagittal_Plane()->Get_Contours_With_Segmentation(criteria2);


        //Print out a list of (the unique) ROI name/number correspondances in the subsegmented contours.
        //for(auto c_it = Subsegmented_New__Style_Contour_Data->contours.begin(); c_it != Subsegmented_New__Style_Contour_Data->contours.end(); ++c_it ){
        //    FUNCINFO("Subsegmented contour with ROI number " << (*c_it).ROI_number << " is named \"" << (*c_it).Raw_ROI_name << "\".");
        //}

        //Plot all the contours found in the DICOM file (one plot.)
        //DICOM_data.contour_data->Plot();

        //Plot all the contours found in the DICOM file (individual plots.)
        //for( auto i : DICOM_data.contour_data->contours ){ //Contour iteration.
        //    i.Plot();
        //}

        //Print out all the (sub)segmented contours.
        //Subsegmented_New__Style_Contour_Data->Plot();

    /*
        //Select a structure, pick out the contour-specific contours, compute the DVH, output it to file.
        {
   
 
          //If given a _clean_ string query.
          Explicator X("./Lexicons/20150925_SGF_and_SGFQ_tags.lexicon");
          long int roi_number = -1;
          for(auto it=Contour_classifications.begin(); it != Contour_classifications.end(); ++it){
              if( X(it->first) == "Left Parotid" ){
                  roi_number = it->second;
                  break;
              }
          }
    
          //If given a _dirty_ string query.
          //long int roi_number = Contour_classifications[the_query];
    
          if(roi_number == -1) FUNCERR("Unable to find desired structure within DICOM file. Is the query malformed?");
    
          Drover specific_data = DICOM_data.Duplicate( DICOM_data.contour_data->Get_Contours_With_Number( roi_number ) );
          specific_data.Meld(VERBOSE);
          auto thedvh = specific_data.Get_DVH();
    
          std::fstream FO(FilenameOut.c_str(), std::ifstream::out);
          FO << "# DVH for structure \"" << Contour_classifications[roi_number] << "\"" << std::endl;
          for(auto it = thedvh.begin(); it != thedvh.end(); ++it){
              FO << it->first << " " << it->second << std::endl;
          }
          FO.close();
    
          FUNCERR("Exiting normally!");
        }
    */

    }
    
//---------------------------------------------------------------------------------------------------------------------
//------------------------------------------------- Visualization -----------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
    //Launch the viewer. This is something we do not recover from, so this is last.
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA);
    glutInitWindowSize(Screen_Pixel_Width,Screen_Pixel_Height);
    glutInitWindowPosition(0,0);
    window = glutCreateWindow("DICOMautomaton Data Visualizer");
    glutDisplayFunc(&DrawGLScene);
    glutIdleFunc(&DrawGLScene);
    glutReshapeFunc(&ReSizeGLScene);
    glutKeyboardFunc(&keyPressed);
    glutMouseFunc(&processMouse);
    InitGL(Screen_Pixel_Width, Screen_Pixel_Height);
    glutMainLoop();

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------- Cleanup --------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
//Try avoid doing anything that will require you to explicitly clean anything up here (ie. please use smart pointers
// where ever they are possible.)

    //Sure hope nothing is here, because glut won't return!
    return 0;
}
