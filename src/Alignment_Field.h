//Alignment_Field.h - A part of DICOMautomaton 2021. Written by hal clark.

#pragma once

#include <optional>
#include <list>
#include <functional>
#include <iosfwd>

#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorImages.h"       //Needed for vec3 class.

namespace AlignViaFieldHelpers {

// Helper functions that are semi-private can be added here instead of as class members,
// which will simplify testing and linking with tests included.

};

class deformation_field {
    private:
        // These are private so they stay synchronized. The adjacency index is rebuilt when the field is altered.
        planar_image_collection<double,double> field; // Vector displacement field. 3 channels required.
        std::optional<planar_image_adjacency<double,double>> adj; // Index used to provide faster look-up and 3D interpolation.

    public:
        // Constructor.
        deformation_field() = delete; // Ensures field always valid -- use optional for empty transform.
        deformation_field(std::istream &is); // Defers to read_from(), but throws on errors.
        deformation_field(planar_image_collection<double,double> &&);

        // Re-constructor.
        void swap_and_rebuild(planar_image_collection<double,double> &);

        // Member functions.
        std::reference_wrapper< const planar_image_collection<double,double> >
            get_imagecoll_crefw() const; // Image array accessor.

        vec3<double> transform(const vec3<double> &v) const;
        void apply_to(point_set<double> &ps) const; // Included for parity with affine_transform class.
        void apply_to(vec3<double> &v) const;       // Included for parity with affine_transform class.

        void apply_to(planar_image<float, double> &img) const;
        void apply_to(planar_image_collection<float, double> &img) const;

        // Serialize and deserialize to a human- and machine-readable format.
        bool write_to( std::ostream &os ) const;
        bool read_from( std::istream &is );
};

