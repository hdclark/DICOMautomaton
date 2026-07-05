#pragma once

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

namespace ygor {
namespace io {
namespace gzip {

using compressor = boost::iostreams::gzip_compressor;
using decompressor = boost::iostreams::gzip_decompressor;
using params = boost::iostreams::gzip_params;

}  // namespace gzip
}  // namespace io
}  // namespace ygor
