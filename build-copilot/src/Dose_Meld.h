//Dose_Meld.h.

#ifndef DOSE_MELD_PROJECT_DICOMAUTOMATON_H_
#define DOSE_MELD_PROJECT_DICOMAUTOMATON_H_

#include <list>
#include <memory>

#include "Structs.h"

class Drover;

//Filter out all non-dose images, presenting a Drover with only dose image_data.
Drover
Isolate_Dose_Data(Drover);

//This routine removes all dose images (i.e., modality = RTDOSE), melds them, and places only the melded result back.
Drover
Meld_Only_Dose_Data(Drover);

//Meld function for an arbitrary collection of user-provided images.
std::list<std::shared_ptr<Image_Array>> 
Meld_Image_Data(const std::list<std::shared_ptr<Image_Array>> &dalist);

//Resamples dose data to ensure no overflow occurs. Is a lossy operation.
std::unique_ptr<Image_Array>
Meld_Equal_Geom_Image_Data(const std::shared_ptr<Image_Array>& A, const std::shared_ptr<Image_Array>& B);

//Resamples dose data AND the smaller of the dose data grids onto the larger. Is a lossy operation.
std::unique_ptr<Image_Array>
Meld_Unequal_Geom_Image_Data(std::shared_ptr<Image_Array> A, const std::shared_ptr<Image_Array>& B);

#endif

