//Alignment_Field.cc - A part of DICOMautomaton 2021. Written by hal clark.

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
#include <functional>

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Structs.h"
#include "Regex_Selectors.h"
#include "Thread_Pool.h"

#include "Alignment_Rigid.h"
#include "Alignment_Field.h"


deformation_field::deformation_field(std::istream &is){
    if(!this->read_from(is)){
        throw std::invalid_argument("Input not understood, refusing to contruct empty field");
    }
}

deformation_field::deformation_field(planar_image_collection<double,double> &&in){
    this->swap_and_rebuild(in);
}

void
deformation_field::swap_and_rebuild(planar_image_collection<double,double> &in){

    // Imbib the images to avoid invalid references making their way into the index.
    this->field.Swap(in);
    try{
        // Ensure the image array is regular. (This will allow us to use a faster postion-to-image lookup.)
        // Also ensure images are present, and every image has three channels.
        std::list<std::reference_wrapper<planar_image<double,double>>> selected_imgs;
        for(auto &img : this->field.images){
            if(img.channels != 3) throw std::invalid_argument("Encountered image without three channels");
            selected_imgs.push_back( std::ref(img) );
        }
        if( selected_imgs.empty() ) throw std::invalid_argument("No images provided");
        if(!Images_Form_Regular_Grid(selected_imgs)){
            throw std::invalid_argument("Images do not form a rectilinear grid. Cannot continue");
        }

        const auto row_unit = this->field.images.front().row_unit.unit();
        const auto col_unit = this->field.images.front().col_unit.unit();
        const auto img_unit = col_unit.Cross(row_unit).unit();

        planar_image_adjacency<double,double> img_adj( {}, { { std::ref(this->field) } }, img_unit );
        this->adj = img_adj;
        
    }catch(const std::exception &){
        this->field.Swap(in);
        throw;
    }
    return;
}

std::reference_wrapper<const planar_image_collection<double,double>>
deformation_field::get_imagecoll_crefw() const {
    return std::cref(this->field);
};

vec3<double> 
deformation_field::transform(const vec3<double> &v) const {
    const double oob = 0.0;
    const auto dx = this->adj.value().trilinearly_interpolate(v, 0, 0.0);
    const auto dy = this->adj.value().trilinearly_interpolate(v, 1, 0.0);
    const auto dz = this->adj.value().trilinearly_interpolate(v, 2, 0.0);
    return v + vec3<double>(dx, dy, dz);
}

void
deformation_field::apply_to(point_set<double> &ps) const {
    for(auto &p : ps.points) p = this->transform(p);
    return;
}

void
deformation_field::apply_to(vec3<double> &v) const {
    v = this->transform(v);
    return;
}

void
deformation_field::apply_to(planar_image<float, double> &img) const {
    throw std::logic_error("Not yet implemented");
    return;
}

void
deformation_field::apply_to(planar_image_collection<float, double> &img) const {
    throw std::logic_error("Not yet implemented");
    return;
}

bool
deformation_field::write_to( std::ostream &os ) const {
    // Maximize precision prior to emitting any floating-point numbers.
    const auto original_precision = os.precision();
    os.precision( std::numeric_limits<double>::max_digits10 );
    
    throw std::logic_error("Not yet implemented");

    os.precision( original_precision );
    os.flush();
    return (!os.fail());
}

bool
deformation_field::read_from( std::istream &is ){
    throw std::logic_error("Not yet implemented");

    return (!is.fail());
}

