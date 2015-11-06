/*

Imebra 2011 build 2011-09-18_22-24-41

Imebra: a C++ Dicom library

Copyright (c) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 by Paolo Brandoli
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors
 and should not be interpreted as representing official policies, either expressed or implied,
 of the Imebra Project.

Imebra is available at http://imebra.com



*/

/*! \file drawBitmap.h
    \brief Declaration of the a class that draw an image into a bitmap.

This file is not included automatically by imebra.h

*/

#if !defined(imebraDrawBitmap_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
#define imebraDrawBitmap_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_

#include "../../base/include/memory.h"
#include "transformsChain.h"
#include <memory>
#include <string.h>

namespace puntoexe
{

	namespace imebra
	{

		/// \addtogroup group_helpers Helpers
		///
		/// @{

		///////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////
		/// \brief Base class used for the exceptions thrown by
		///         drawBitmap.
		///
		///////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////
		class drawBitmapException: public std::runtime_error
		{
		public:
			drawBitmapException(const std::string& message): std::runtime_error(message){}
		};

		///////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////
		/// \brief This exception is thrown by
		///         declareBitmapType() if the image's area that
		///         has to be generated is not valid.
		///
		///////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////
		class drawBitmapExceptionInvalidArea: public drawBitmapException
		{
		public:
			drawBitmapExceptionInvalidArea(const std::string& message): drawBitmapException(message){}
		};


		enum tDrawBitmapType
		{
			drawBitmapRGB,
			drawBitmapBGR
		};

		///////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////
		/// \brief This class takes an image as an input and
		///         returns an 8 bit RGB bitmap of the requested
		///         image's area.
		///
		///////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////
		class drawBitmap: public baseObject
		{
		public:
			/// \brief Constructor.
			///
			/// @param sourceImage  the input image that has to be
			///                      rendered
			/// @param transformsChain the list of transforms to be
			///                      applied to the image before it
			///                      is rendered. Can be null.
			///                     The transformation to RGB and
			///                      high bit shift are applied
			///                      automatically by this class
			///
			///////////////////////////////////////////////////////////
			drawBitmap(ptr<image> sourceImage, ptr<transforms::transformsChain> transformsChain);

			/// \brief Renders the image specified in the constructor
			///         into an RGB or BGR buffer.
			///
			/// The caller can pass to the function a memory object
			///  that will be used to store the RGB/BGR buffer,
			///  otherwise the function will allocate a new one.
			///
			/// Each row of pixels in the RGB/BGR buffer will be
			///  aligned according to the template parameter
			///  rowAlignBytes
			///
			/// \image html drawbitmap.png "drawbitmap"
			/// \image latex drawbitmap.png "drawbitmap" width=15cm
			///
			/// The figure illustrates how the getBitmap() method
			///  works:
			/// -# the image is resized according to the parameters
			///        totalWidthPixels and totalHeightPixels
			/// -# the area specified by visibleTopLeftX,
			///    visibleTopLeftY - visibleBottomRightX,
			///    visibleBottomRightY is rendered into the buffer
			///
			/// Please note that the rendering algorithm achieves the
			///  described results without actually resizing the image.
			///
			/// @tparam drawBitmapType The RGB order. Must be
			///                         drawBitmapBGR for BMP images
			/// @tparam rowAlignBytes  the boundary alignment of each
			///                         row. Must be 4 for BMP images
			/// @param totalWidthPixels the width of the magnified or
			///                          shrunken image in pixels
			///                          (magnified width on the
			///                          figure "drawbitmap")
			/// @param totalHeightPixels the height of the magnified
			///                           or shrunken image in pixels
			///                           (magnified height on the
			///                           figure "drawbitmap")
			/// @param visibleTopLeftX  the X coordinate of the top
			///                          left corner of image area that
			///                          has to be rendered
			///                          (visible top left magnified X
			///                           on the figure "drawbitmap")
			/// @param visibleTopLeftY  the Y coordinate of the top
			///                          left corner of image area that
			///                          has to be rendered
			///                          (visible top left magnified Y
			///                           on the figure "drawbitmap")
			/// @param visibleBottomRightX the X coordinate of the
			///                          bottom right corner of image
			///                          area that has to be rendered
			///                          (visible bottom right
			///                           magnified X on the figure
			///                           "drawbitmap")
			/// @param visibleBottomRightY the Y coordinate of the
			///                          bottom right corner of image
			///                          area that has to be rendered
			///                          (visible bottom right
			///                           magnified Y on the figure
			///                           "drawbitmap")
			/// @param reuseMemory      a pointer to a memory object
			///                          that must be used to store the
			///                          output buffer. Can be null
			/// @return the memory object in which the output buffer
			///          is stored. Is the same object specified in
			///          reuseMemory or a new object if reuseMemory
			///          is null
			///
			///////////////////////////////////////////////////////////
			template <tDrawBitmapType drawBitmapType, int rowAlignBytes>
					ptr<memory> getBitmap(imbxInt32 totalWidthPixels, imbxInt32 totalHeightPixels,
										  imbxInt32 visibleTopLeftX, imbxInt32 visibleTopLeftY, imbxInt32 visibleBottomRightX, imbxInt32 visibleBottomRightY,
										  ptr<memory> reuseMemory)
			{
				PUNTOEXE_FUNCTION_START(L"drawBitmap::getBitmap");

				// Just return if there is nothing to show
				///////////////////////////////////////////////////////////
				if(visibleTopLeftX == visibleBottomRightX || visibleTopLeftY == visibleBottomRightY)
				{
					if(reuseMemory != 0)
					{
						reuseMemory->resize(0);
					}
					return reuseMemory;
				}

				// Check if the image is visible in the specified area
				///////////////////////////////////////////////////////////
				if(
						visibleBottomRightX > totalWidthPixels ||
						visibleBottomRightY > totalHeightPixels ||
						visibleTopLeftX < 0 ||
						visibleTopLeftY < 0 ||
						visibleTopLeftX > visibleBottomRightX ||
						visibleTopLeftY > visibleBottomRightY
						)
				{
					PUNTOEXE_THROW(drawBitmapExceptionInvalidArea, "Destination area not valid");
				}

				// Calculate the row' size, in bytes
				///////////////////////////////////////////////////////////
				imbxUint32 rowSizeBytes = ((visibleBottomRightX - visibleTopLeftX) * 3 + rowAlignBytes - 1) / rowAlignBytes;
				rowSizeBytes *= rowAlignBytes;

				// Allocate the memory for the final bitmap
				///////////////////////////////////////////////////////////
				imbxUint32 memorySize(rowSizeBytes * (visibleBottomRightY - visibleTopLeftY));
				if(reuseMemory == 0)
				{
					reuseMemory = memoryPool::getMemoryPool()->getMemory(memorySize);
				}
				else
				{
					reuseMemory->resize(memorySize);
				}

				// Find the multiplier that make the image bigger than
				//  the rendering area
				///////////////////////////////////////////////////////////
				imbxUint32 imageSizeX, imageSizeY;
				m_image->getSize(&imageSizeX, &imageSizeY);

				imbxUint8 leftShiftX(0), leftShiftY(0);
				imbxUint32 maskX(0), maskY(0);
				while( (imageSizeX << leftShiftX) < (imbxUint32)totalWidthPixels)
				{
					++leftShiftX;
					maskX <<= 1;
					++maskX;
				}
				while( (imageSizeY << leftShiftY) < (imbxUint32)totalHeightPixels)
				{
					++leftShiftY;
					maskY <<= 1;
					++maskY;
				}

				// Allocate an horizontal buffer that stores the pixels
				//  average colors and a buffer that indicates the pixels
				//  in the source image mapped to the final bitmap
				///////////////////////////////////////////////////////////
				imbxUint32 destBitmapWidth(visibleBottomRightX - visibleTopLeftX);
				std::auto_ptr<imbxInt32> averagePixels(new imbxInt32[destBitmapWidth * 4]);
				std::auto_ptr<imbxUint32> sourcePixelIndex(new imbxUint32[destBitmapWidth + 1]);
				for(imbxInt32 scanPixelsX = visibleTopLeftX; scanPixelsX != visibleBottomRightX + 1; ++scanPixelsX)
				{
					sourcePixelIndex.get()[scanPixelsX - visibleTopLeftX] = scanPixelsX * (imageSizeX << leftShiftX) / totalWidthPixels;
				}

				// Get the index of the first and last+1 pixel to be
				//  displayed
				///////////////////////////////////////////////////////////
				imbxInt32 firstPixelX(sourcePixelIndex.get()[0]);
				imbxInt32 lastPixelX(sourcePixelIndex.get()[visibleBottomRightX - visibleTopLeftX]);

				// If a transform chain is active then allocate a temporary
				//  output image
				///////////////////////////////////////////////////////////
				imbxUint32 rowSize, channelSize, channelsNumber;
				ptr<image> sourceImage(m_image);

				// Retrieve the final bitmap's buffer
				///////////////////////////////////////////////////////////
				imbxUint8* pFinalBuffer = (imbxUint8*)(reuseMemory->data());
				imbxInt32 nextRowGap = rowSizeBytes - destBitmapWidth * 3;

				// First Y pixel not transformed by the transforms chain
				///////////////////////////////////////////////////////////
				imbxInt32 transformChainStartY(0), transformChainNextY(0);

				imbxUint32 sourceHeight;
				imbxUint32 sourceWidth;
				if(m_transformsChain->isEmpty())
				{
					sourceHeight = imageSizeY;
					sourceWidth = imageSizeX;
				}
				else
				{

					sourceWidth = (lastPixelX >> leftShiftX) - (firstPixelX >> leftShiftX) + 1;
					if((firstPixelX >> leftShiftX) + sourceWidth > imageSizeX)
					{
						sourceWidth = imageSizeX - (firstPixelX >> leftShiftX);
					}
					sourceHeight = 65536 / (sourceWidth * 3);
					if(sourceHeight < 1)
					{
						sourceHeight = 1;
					}
					if(sourceHeight > imageSizeY)
					{
						sourceHeight = imageSizeY;
					}
					sourceImage = new image;
					sourceImage->create(sourceWidth, sourceHeight, image::depthU8, L"RGB", 7);
				}


				// Scan all the final bitmap's rows
				///////////////////////////////////////////////////////////
				for(imbxInt32 scanY = visibleTopLeftY; scanY != visibleBottomRightY; ++scanY)
				{
					::memset(averagePixels.get(), 0, destBitmapWidth * 4 * sizeof(averagePixels.get()[0]));

					// Scan all the image's rows that go in the bitmap's row
					///////////////////////////////////////////////////////////
					imbxInt32 firstPixelY = scanY * (imageSizeY << leftShiftY) / totalHeightPixels;
					imbxInt32 lastPixelY = (scanY + 1) * (imageSizeY << leftShiftY) / totalHeightPixels;
					for(imbxInt32 scanImageY = firstPixelY; scanImageY != lastPixelY; /* increased in the loop */)
					{
						imbxInt32 currentImageY = (scanImageY >> leftShiftY);
						imbxInt32* pAveragePointer = averagePixels.get();
						imbxUint32* pNextSourceXIndex = sourcePixelIndex.get();

						imbxUint8* pImagePointer(0);
						imbxUint8* imageMemory(0);

						ptr<handlers::dataHandlerNumericBase> imageHandler;
						if(m_transformsChain->isEmpty())
						{
							imageHandler = sourceImage->getDataHandler(false, &rowSize, &channelSize, &channelsNumber);
							imageMemory = imageHandler->getMemoryBuffer();
							pImagePointer = &(imageMemory[currentImageY * imageSizeX * 3 + ((*pNextSourceXIndex) >> leftShiftX) * 3]);
						}
						else
						{
							if(currentImageY >= transformChainNextY)
							{
								transformChainNextY = currentImageY + sourceHeight;
								if(transformChainNextY > (imbxInt32)imageSizeY)
								{
									transformChainNextY = imageSizeY;
								}
								m_transformsChain->runTransform(m_image, firstPixelX >> leftShiftX, currentImageY, sourceWidth, transformChainNextY - currentImageY, sourceImage, 0, 0);
								transformChainStartY = currentImageY;
							}
							imageHandler = sourceImage->getDataHandler(false, &rowSize, &channelSize, &channelsNumber);
							imageMemory = imageHandler->getMemoryBuffer();

							pImagePointer = &(imageMemory[(currentImageY - transformChainStartY) * sourceWidth * 3]);
						}

						imbxInt32 scanYBlock ( (scanImageY & (~maskY)) + ((imbxInt32)1 << leftShiftY) );
						if(scanYBlock > lastPixelY)
						{
							scanYBlock = lastPixelY;
						}
						imbxInt32 numRows(scanYBlock - scanImageY);
						scanImageY += numRows;

						if(numRows == 1)
						{
							for(imbxInt32 scanX (destBitmapWidth); scanX != 0; --scanX)
							{
								for(imbxUint32 scanImageX = *(pNextSourceXIndex++); scanImageX != *pNextSourceXIndex; ++scanImageX)
								{
									++(*pAveragePointer);
									*(++pAveragePointer) += *pImagePointer;
									*(++pAveragePointer) += *(++pImagePointer);
									*(++pAveragePointer) += *(++pImagePointer);
									pAveragePointer -= 3;
									if( (scanImageX & maskX) != 0)
									{
										pImagePointer -= 2;
										continue;
									}
									++pImagePointer;
								}
								pAveragePointer += 4;
							}
						}
						else
						{
							for(imbxInt32 scanX (destBitmapWidth); scanX != 0; --scanX)
							{
								for(imbxUint32 scanImageX = *(pNextSourceXIndex++); scanImageX != *pNextSourceXIndex; ++scanImageX)
								{
									*pAveragePointer += numRows;
									*(++pAveragePointer) += *pImagePointer  * numRows;
									*(++pAveragePointer) += *(++pImagePointer)  * numRows;
									*(++pAveragePointer) += *(++pImagePointer)  * numRows;
									pAveragePointer -= 3;
									if( (scanImageX & maskX) != 0)
									{
										pImagePointer -= 2;
										continue;
									}
									++pImagePointer;
								}
								pAveragePointer += 4;
							}
						}
					}

					// Copy the average to the bitmap
					imbxInt32* pAveragePointer = averagePixels.get();
					imbxUint32 counter;

					if(drawBitmapType == drawBitmapRGB)
					{
						for(imbxInt32 scanX (destBitmapWidth); scanX != 0; --scanX)
						{
							counter = (imbxUint32)*(pAveragePointer++);
							*(pFinalBuffer++) = (imbxUint8) (((imbxUint32)*(pAveragePointer++) / counter) & 0xff);
							*(pFinalBuffer++) = (imbxUint8) (((imbxUint32)*(pAveragePointer++) / counter) & 0xff);
							*(pFinalBuffer++) = (imbxUint8) (((imbxUint32)*(pAveragePointer++) / counter) & 0xff);
						}
					}
					else
					{
						for(imbxInt32 scanX (destBitmapWidth); scanX != 0; --scanX)
						{
							counter = (imbxUint32)*(pAveragePointer++);
							imbxUint32 r = (imbxUint8) (((imbxUint32)*(pAveragePointer++) / counter) & 0xff);
							imbxUint32 g = (imbxUint8) (((imbxUint32)*(pAveragePointer++) / counter) & 0xff);
							imbxUint32 b = (imbxUint8) (((imbxUint32)*(pAveragePointer++) / counter) & 0xff);
							*(pFinalBuffer++) = (imbxUint8)b;
							*(pFinalBuffer++) = (imbxUint8)g;
							*(pFinalBuffer++) = (imbxUint8)r;
						}
					}
					pFinalBuffer += nextRowGap;
				}

				return reuseMemory;

				PUNTOEXE_FUNCTION_END();

			}

		protected:
			ptr<image> m_image;

			ptr<image> m_finalImage;

			// Transform that calculates an 8 bit per channel RGB image
			ptr<transforms::transformsChain> m_transformsChain;
		};

		/// @}

	} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDrawBitmap_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
