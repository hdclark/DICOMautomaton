/*

Imebra 2011 build 2012-12-19_20-05-13

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

/*! \file transformHighBit.h
    \brief Declaration of the class transformHighBit.

*/

#if !defined(imebraTransformHighBit_8347C70F_1FC8_4df8_A887_8DE9E968B2CF__INCLUDED_)
#define imebraTransformHighBit_8347C70F_1FC8_4df8_A887_8DE9E968B2CF__INCLUDED_

#include "transform.h"
#include "colorTransformsFactory.h"

namespace puntoexe
{

namespace imebra
{

namespace transforms
{

/// \addtogroup group_transforms
///
/// @{

class transformHighBitException: public transformException
{
public:
		transformHighBitException(const std::string& message): transformException(message){}
};

class transformHighBitDifferentColorSpaces: public transformHighBitException
{
public:
    transformHighBitDifferentColorSpaces(const std::string& message): transformHighBitException(message){}
};


///////////////////////////////////////////////////////////
/// \brief Shift the image's content to adapt it to the
///         specified high bit.
///
/// In order to use this transform, both the input and
///  the output image must be defined by calling
///  declareInputImage() and declareOutputImage().
///
/// The input image is copied into the output image but
///  all the values are shifted to be fitted in the
///  output image's high bit settings.
///
///////////////////////////////////////////////////////////
class transformHighBit: public transformHandlers
{
public:
        ptr<image> allocateOutputImage(ptr<image> pInputImage, imbxUint32 width, imbxUint32 height) override;

        DEFINE_RUN_TEMPLATE_TRANSFORM;

        template <class inputType, class outputType>
        void templateTransform(
            inputType* inputHandlerData, size_t /* inputHandlerSize */, imbxUint32 inputHandlerWidth, const std::wstring& inputHandlerColorSpace,
            ptr<palette> /* inputPalette */,
            imbxInt32 inputHandlerMinValue, imbxUint32 inputHandlerNumValues,
            imbxInt32 inputTopLeftX, imbxInt32 inputTopLeftY, imbxInt32 inputWidth, imbxInt32 inputHeight,
            outputType* outputHandlerData, size_t /* outputHandlerSize */, imbxInt32 outputHandlerWidth, const std::wstring& outputHandlerColorSpace,
            ptr<palette> /* outputPalette */,
            imbxInt32 outputHandlerMinValue, imbxUint32 outputHandlerNumValues,
            imbxInt32 outputTopLeftX, imbxInt32 outputTopLeftY)

        {
            if(colorTransforms::colorTransformsFactory::normalizeColorSpace(inputHandlerColorSpace) !=
               colorTransforms::colorTransformsFactory::normalizeColorSpace(outputHandlerColorSpace))
            {
                throw transformHighBitDifferentColorSpaces("The input and output image must have the same color space");
            }

            imbxInt32 numChannels(colorTransforms::colorTransformsFactory::getNumberOfChannels(inputHandlerColorSpace));

            inputType* pInputMemory(inputHandlerData);
            outputType* pOutputMemory(outputHandlerData);

            pInputMemory += (inputTopLeftY * inputHandlerWidth + inputTopLeftX) * numChannels;
            pOutputMemory += (outputTopLeftY * outputHandlerWidth + outputTopLeftX) * numChannels;

            if(inputHandlerNumValues == outputHandlerNumValues)
            {
                imbxInt32 offset(inputHandlerMinValue - outputHandlerMinValue);
                for(; inputHeight != 0; --inputHeight)
                {
                    for(int scanPixels(inputWidth * numChannels); scanPixels != 0; --scanPixels)
                    {
                        *(pOutputMemory++) = (outputType)((imbxInt32)*(pInputMemory++) - offset);
                    }
                    pInputMemory += (inputHandlerWidth - inputWidth) * numChannels;
                    pOutputMemory += (outputHandlerWidth - inputWidth) * numChannels;
                }
            }
            else
            {
                for(; inputHeight != 0; --inputHeight)
                {
                    for(int scanPixels(inputWidth * numChannels); scanPixels != 0; --scanPixels)
                    {
                        *pOutputMemory++ = (outputType)((((imbxInt32)*(pInputMemory++) - inputHandlerMinValue) * outputHandlerNumValues) / inputHandlerNumValues + outputHandlerMinValue);
                    }
                    pInputMemory += (inputHandlerWidth - inputWidth) * numChannels;
                    pOutputMemory += (outputHandlerWidth - inputWidth) * numChannels;
                }
            }
        }

};

/// @}

} // namespace transforms

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraTransformHighBit_8347C70F_1FC8_4df8_A887_8DE9E968B2CF__INCLUDED_)
