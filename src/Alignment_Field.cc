//Alignment_Field.cc - A part of DICOMautomaton 2021. Written by hal clark.

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

#include "Structs.h"
#include "Regex_Selectors.h"
#include "Thread_Pool.h"

#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorStats.h"        //Needed for Stats:: namespace.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "Alignment_Rigid.h"
#include "Alignment_Field.h"


deformation_field::deformation_field(const planar_image_collection<double,double> &in){
    this->field = in;
}

vec3<double> 
deformation_field::transform(const vec3<double> &v) const {
    throw std::logic_error("Not yet implemented");
    return v;
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

