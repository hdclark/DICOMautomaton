//Whole_Match.h.

#ifndef WHOLE_MATCH_DICOMAUTOMATON_H_
#define WHOLE_MATCH_DICOMAUTOMATON_H_

#include <string>
#include <map>

#include "Explicator.h"
#include "Demarcator.h"

#include "Structs.h"

std::map<std::string, std::string> Whole_Match(Explicator &X, Demarcator &Y, const Contour_Data &cd);


#endif
