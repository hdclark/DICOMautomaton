//Lexicon_Loader.h.

#pragma once

#include <string>    

// This function attempts to locate a lexicon file. If none are available, an empty string is returned.
std::string Locate_Lexicon_File();

// This function creates a default lexicon file in a temporary location. The full path is returned.
std::string Create_Default_Lexicon_File();

