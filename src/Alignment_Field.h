//Alignment_Field.h - A part of DICOMautomaton 2021. Written by hal clark.

#pragma once

#include <optional>
#include <iosfwd>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorImages.h"       //Needed for vec3 class.


class deformation_field {
    public:
        planar_image_collection<double,double> field;

        // Constructor.
        deformation_field() = delete;
        deformation_field(const planar_image_collection<double,double> &);

        // Member functions.
        vec3<double> transform(const vec3<double> &v) const;
        void apply_to(point_set<double> &ps) const; // Included for parity with affine_transform class.
        void apply_to(vec3<double> &v) const;       // Included for parity with affine_transform class.

        void apply_to(planar_image<float, double> &img) const;
        void apply_to(planar_image_collection<float, double> &img) const;

        // Serialize and deserialize to a human- and machine-readable format.
        bool write_to( std::ostream &os ) const;
        bool read_from( std::istream &is );
};

