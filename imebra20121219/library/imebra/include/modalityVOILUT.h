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

/*! \file modalityVOILUT.h
    \brief Declaration of the class modalityVOILUT.

*/

#if !defined(imebraModalityVOILUT_8347C70F_1FC8_4df8_A887_8DE9E968B2CF__INCLUDED_)
#define imebraModalityVOILUT_8347C70F_1FC8_4df8_A887_8DE9E968B2CF__INCLUDED_

#include "transform.h"
#include "image.h"
#include "dataSet.h"
#include "LUT.h"
#include "colorTransformsFactory.h"


///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{

namespace transforms
{

class modalityVOILUTException;

/// \addtogroup group_transforms
///
/// @{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This class transforms the pixel values of the
///         image retrieved from the dataset into values
///         that are meaningful to th application.
///
/// For instance, the original pixel values could store
///  a device specific value that has a meaning only when
///  used by the device that generated it: this transform
///  uses the modality VOI/LUT defined in the dataset to
///  convert the original values into optical density
///  or other known measure units.
///
/// If the dataset doesn't define any modality VOI/LUT
///  transformation, then the input image is simply copied
///  into the output image.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class modalityVOILUT: public transformHandlers
{
public:
	/// \brief Constructor.
	///
	/// @param pDataSet the dataSet from which the input
	///         images come from
	///
	///////////////////////////////////////////////////////////
    modalityVOILUT(ptr<dataSet> pDataSet);

	DEFINE_RUN_TEMPLATE_TRANSFORM;

	template <class inputType, class outputType>
			void templateTransform(
					inputType* inputHandlerData, size_t /* inputHandlerSize */, imbxUint32 inputHandlerWidth, const std::wstring& inputHandlerColorSpace,
					ptr<palette> /* inputPalette */,
					imbxInt32 /* inputHandlerMinValue */, imbxUint32 /* inputHandlerNumValues */,
					imbxInt32 inputTopLeftX, imbxInt32 inputTopLeftY, imbxInt32 inputWidth, imbxInt32 inputHeight,
					outputType* outputHandlerData, size_t /* outputHandlerSize */, imbxInt32 outputHandlerWidth, const std::wstring& outputHandlerColorSpace,
					ptr<palette> /* outputPalette */,
					imbxInt32 /* outputHandlerMinValue */, imbxUint32 /* outputHandlerNumValues */,
					imbxInt32 outputTopLeftX, imbxInt32 outputTopLeftY)
	{
		PUNTOEXE_FUNCTION_START(L"modalityVOILUT::templateTransform");
		if(!colorTransforms::colorTransformsFactory::isMonochrome(inputHandlerColorSpace) || !colorTransforms::colorTransformsFactory::isMonochrome(outputHandlerColorSpace))
		{
			PUNTOEXE_THROW(modalityVOILUTException, "modalityVOILUT can process only monochromatic images");
		}
		inputType* pInputMemory(inputHandlerData);
		outputType* pOutputMemory(outputHandlerData);

		pInputMemory += inputTopLeftY * inputHandlerWidth + inputTopLeftX;
		pOutputMemory += outputTopLeftY * outputHandlerWidth + outputTopLeftX;

		//
		// Modality LUT found
		//
		///////////////////////////////////////////////////////////
		if(m_voiLut != 0 && m_voiLut->getSize() != 0 && m_voiLut->checkValidDataRange())
		{
			for(; inputHeight != 0; --inputHeight)
			{
				for(int scanPixels(inputWidth); scanPixels != 0; --scanPixels)
				{
					*(pOutputMemory++) = (outputType) ( m_voiLut->mappedValue((imbxInt32) *(pInputMemory++)) );
				}
				pInputMemory += (inputHandlerWidth - inputWidth);
				pOutputMemory += (outputHandlerWidth - inputWidth);
			}
			return;
		}

		//
		// Modality LUT not found
		//
		///////////////////////////////////////////////////////////

		// Retrieve the intercept/scale pair
		///////////////////////////////////////////////////////////
		for(; inputHeight != 0; --inputHeight)
		{
			for(int scanPixels(inputWidth); scanPixels != 0; --scanPixels)
			{
				*(pOutputMemory++) = (outputType)((double)(*(pInputMemory++)) * m_rescaleSlope + m_rescaleIntercept + 0.5);
			}
			pInputMemory += (inputHandlerWidth - inputWidth);
			pOutputMemory += (outputHandlerWidth - inputWidth);
		}
		PUNTOEXE_FUNCTION_END();
	}

	virtual bool isEmpty();

	virtual ptr<image> allocateOutputImage(ptr<image> pInputImage, imbxUint32 width, imbxUint32 height);

private:
    ptr<dataSet> m_pDataSet;
    ptr<lut> m_voiLut;
    double m_rescaleIntercept;
    double m_rescaleSlope;
	bool m_bEmpty;
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown by modalityVOILUT
///         when the images passed to the transform are
///         not monochromatic.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class modalityVOILUTException: public transformException
{
public:
	modalityVOILUTException(const std::string& message): transformException(message){}
};

/// @}

} // namespace transforms

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraModalityVOILUT_8347C70F_1FC8_4df8_A887_8DE9E968B2CF__INCLUDED_)
