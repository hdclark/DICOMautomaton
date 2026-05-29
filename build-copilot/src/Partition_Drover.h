// Partition_Drover.h.

#pragma once

#include <list>
#include <set>
#include <iterator>
#include <optional>
#include <map>
#include <string>
#include <utility>

#include "Structs.h"

// -------------------------------------------------------------------------------------------------------------------
// ------------------------------------ Key-value based metadata partitioning ----------------------------------------
// -------------------------------------------------------------------------------------------------------------------
struct Partitioned_Drover {
    // The metadata keys used to make the partitions.
    std::set<std::string> keys_common;

    // External index from the metadata values and partitions.
    //
    // This is external because so the ordering can remain the same.
    std::map< std::set<std::string>, std::list<Drover>::iterator > index;

    // The partitions that had all-valid metadata keys. All objects within each of these partitions have the same
    // metadata key-value pair at the time of partitioning, but can be altered by the user arbitrarily before being
    // combined back together.
    std::list< Drover > partitions;

    // The partitions that did not have all-valid metadata keys.
    std::optional<Drover> na_partition;
};

// Split a Drover object according to the provided metadata keys.
Partitioned_Drover
Partition_Drover( Drover &,
                  std::set<std::string> keys_common );

// Re-combine a Partitioned_Drover into a regular Drover.
Drover
Combine_Partitioned_Drover( Partitioned_Drover & );


// -------------------------------------------------------------------------------------------------------------------
// ---------------------------------- RTPlan partitioning based on DICOM linkage -------------------------------------
// -------------------------------------------------------------------------------------------------------------------
struct Drover_Selection {
    Drover select; // The relevant parts.
    Drover extras; // The non-relevant parts.
};

// Split a Drover object into two parts based on relevance to a given RTPlan.
Drover_Selection
Select_Drover( Drover &, std::list<std::shared_ptr<RTPlan>>::iterator );

// Re-combine a split Drover object, handling additions/removals/modifications.
Drover
Recombine_Selected_Drover( Drover_Selection );


