//Dose_Meld.h.

#ifndef DOSE_MELD_PROJECT_DICOMAUTOMATON_H_
#define DOSE_MELD_PROJECT_DICOMAUTOMATON_H_

#include <list>
#include <memory>

#include "Structs.h"

class Dose_Array;

std::list<std::shared_ptr<Dose_Array>>  Meld_Dose_Data(const std::list<std::shared_ptr<Dose_Array>> &dalist);

//Resamples dose data to ensure no overflow occurs. Is a lossy operation.
std::unique_ptr<Dose_Array> Meld_Equal_Geom_Dose_Data(std::shared_ptr<Dose_Array> A, std::shared_ptr<Dose_Array> B);

//Resamples dose data AND the smaller of the dose data grids onto the larger. Is a lossy operation.
std::unique_ptr<Dose_Array> Meld_Unequal_Geom_Dose_Data(std::shared_ptr<Dose_Array> A, std::shared_ptr<Dose_Array> B);
#endif

