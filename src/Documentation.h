//Documentation.h.

#pragma once

#include <cstdint>
#include <string>
#include <iostream>

void Emit_Op_Documentation(const std::string &op_name_or_alias,
                           std::ostream &os,
                           const std::string &bulleta = "- ",
                           const std::string &bulletb = "  ",
                           const std::string &nobullet = "",
                           const std::string &nolinebreak = "",
                           const int64_t max_width = 120 );

void Emit_Documentation(std::ostream &os,
                        const std::string &bulleta = "- ",
                        const std::string &bulletb = "  ",
                        const std::string &nobullet = "",
                        const std::string &nolinebreak = "",
                        const int64_t max_width = 120 );

