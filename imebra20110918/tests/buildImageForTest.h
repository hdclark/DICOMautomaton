#if !defined(imebraBuildImageForTest_13D4DE03_AD80_4e0a_A01A_54D77C9AF0E8__INCLUDED_)
#define imebraBuildImageForTest_13D4DE03_AD80_4e0a_A01A_54D77C9AF0E8__INCLUDED_

#include "../library/imebra/include/imebra.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

	puntoexe::ptr<puntoexe::imebra::image> buildImageForTest(
		imbxUint32 pixelsX, 
		imbxUint32 pixelsY, 
		puntoexe::imebra::image::bitDepth depth,
		imbxUint32 highBit, 
		double sizeX, 
		double sizeY, 
		std::wstring colorSpace, 
		imbxUint32 continuity);

	double compareImages(ptr<image> image0, ptr<image> image1);


} // namespace tests

} // namespace imebra

} // namespace puntoexe


#endif // #if !defined(imebraBuildImageForTest_13D4DE03_AD80_4e0a_A01A_54D77C9AF0E8__INCLUDED_)
