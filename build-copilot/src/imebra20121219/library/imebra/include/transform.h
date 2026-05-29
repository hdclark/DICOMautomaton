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

/*! \file transform.h
    \brief Declaration of the base class used by all the transforms.
 
*/

#if !defined(imebraTransform_5DB89BFD_F105_45e7_B9D9_3756AC93C821__INCLUDED_)
#define imebraTransform_5DB89BFD_F105_45e7_B9D9_3756AC93C821__INCLUDED_

#include "../../base/include/baseObject.h"
#include "dataHandlerNumeric.h"
#include "image.h"


#define DEFINE_RUN_TEMPLATE_TRANSFORM \
template <typename inputType>\
void runTemplateTransform1(\
    inputType* inputData, size_t inputDataSize, imbxUint32 inputHandlerWidth, const std::wstring& inputHandlerColorSpace,\
    puntoexe::ptr<puntoexe::imebra::palette> inputPalette,\
    imbxInt32 inputHandlerMinValue, imbxUint32 inputHandlerNumValues,\
    imbxInt32 inputTopLeftX, imbxInt32 inputTopLeftY, imbxInt32 inputWidth, imbxInt32 inputHeight,\
    puntoexe::ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> outputHandler, imbxInt32 outputHandlerWidth, const std::wstring& outputHandlerColorSpace,\
    puntoexe::ptr<puntoexe::imebra::palette> outputPalette,\
    imbxInt32 outputHandlerMinValue, imbxUint32 outputHandlerNumValues,\
    imbxInt32 outputTopLeftX, imbxInt32 outputTopLeftY)\
{\
        HANDLER_CALL_TEMPLATE_FUNCTION_WITH_PARAMS(runTemplateTransform2, outputHandler, \
                        inputData, inputDataSize, inputHandlerWidth, inputHandlerColorSpace,\
			inputPalette,\
                        inputHandlerMinValue, inputHandlerNumValues,\
			inputTopLeftX, inputTopLeftY, inputWidth, inputHeight,\
			outputHandlerWidth, outputHandlerColorSpace,\
                        outputPalette,\
                        outputHandlerMinValue, outputHandlerNumValues,\
			outputTopLeftX, outputTopLeftY);\
}\
\
void runTemplateTransform(\
    puntoexe::ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> inputHandler, imbxUint32 inputHandlerWidth, const std::wstring& inputHandlerColorSpace,\
    puntoexe::ptr<puntoexe::imebra::palette> inputPalette,\
    imbxInt32 inputHandlerMinValue, imbxUint32 inputHandlerNumValues,\
    imbxInt32 inputTopLeftX, imbxInt32 inputTopLeftY, imbxInt32 inputWidth, imbxInt32 inputHeight,\
    puntoexe::ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> outputHandler, imbxInt32 outputHandlerWidth, const std::wstring& outputHandlerColorSpace,\
    puntoexe::ptr<puntoexe::imebra::palette> outputPalette,\
    imbxInt32 outputHandlerMinValue, imbxUint32 outputHandlerNumValues,\
    imbxInt32 outputTopLeftX, imbxInt32 outputTopLeftY)\
{\
        HANDLER_CALL_TEMPLATE_FUNCTION_WITH_PARAMS(runTemplateTransform1, inputHandler, \
                        inputHandlerWidth, inputHandlerColorSpace,\
                        inputPalette,\
			inputHandlerMinValue, inputHandlerNumValues,\
			inputTopLeftX, inputTopLeftY, inputWidth, inputHeight,\
			outputHandler, outputHandlerWidth, outputHandlerColorSpace,\
                        outputPalette,\
                        outputHandlerMinValue, outputHandlerNumValues,\
			outputTopLeftX, outputTopLeftY);\
}\
\
template <typename outputType, typename inputType>\
void runTemplateTransform2(\
    outputType* outputData, size_t outputDataSize, \
    inputType* inputData, size_t inputDataSize, \
    imbxUint32 inputHandlerWidth, const std::wstring& inputHandlerColorSpace,\
    puntoexe::ptr<puntoexe::imebra::palette> inputPalette,\
    imbxInt32 inputHandlerMinValue, imbxUint32 inputHandlerNumValues,\
    imbxInt32 inputTopLeftX, imbxInt32 inputTopLeftY, imbxInt32 inputWidth, imbxInt32 inputHeight,\
    imbxInt32 outputHandlerWidth, const std::wstring& outputHandlerColorSpace,\
    puntoexe::ptr<puntoexe::imebra::palette> outputPalette,\
    imbxInt32 outputHandlerMinValue, imbxUint32 outputHandlerNumValues,\
    imbxInt32 outputTopLeftX, imbxInt32 outputTopLeftY)\
{\
        templateTransform( \
            inputData, inputDataSize, inputHandlerWidth, inputHandlerColorSpace, \
            inputPalette, \
            inputHandlerMinValue, inputHandlerNumValues, \
            inputTopLeftX, inputTopLeftY, inputWidth, inputHeight, \
            outputData, outputDataSize, outputHandlerWidth, outputHandlerColorSpace, \
            outputPalette, \
            outputHandlerMinValue, outputHandlerNumValues, \
            outputTopLeftX, outputTopLeftY);\
}\
\
virtual void runTransformHandlers(\
    puntoexe::ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> inputHandler, imbxUint32 inputHandlerWidth, const std::wstring& inputHandlerColorSpace,\
    puntoexe::ptr<puntoexe::imebra::palette> inputPalette,\
    imbxInt32 inputHandlerMinValue, imbxUint32 inputHandlerNumValues,\
    imbxInt32 inputTopLeftX, imbxInt32 inputTopLeftY, imbxInt32 inputWidth, imbxInt32 inputHeight,\
    puntoexe::ptr<puntoexe::imebra::handlers::dataHandlerNumericBase> outputHandler, imbxInt32 outputHandlerWidth, const std::wstring& outputHandlerColorSpace,\
    puntoexe::ptr<puntoexe::imebra::palette> outputPalette,\
    imbxInt32 outputHandlerMinValue, imbxUint32 outputHandlerNumValues,\
    imbxInt32 outputTopLeftX, imbxInt32 outputTopLeftY)\
{\
    runTemplateTransform(inputHandler, inputHandlerWidth, inputHandlerColorSpace, inputPalette, inputHandlerMinValue, inputHandlerNumValues,\
            inputTopLeftX, inputTopLeftY, inputWidth, inputHeight,\
            outputHandler, outputHandlerWidth, outputHandlerColorSpace, outputPalette, outputHandlerMinValue, outputHandlerNumValues,\
            outputTopLeftX, outputTopLeftY);\
}

///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{

namespace handlers
{
    class dataHandlerNumericBase;
}

class image;
class dataSet;
class lut;

/// \namespace transforms
/// \brief All the transforms are declared in this
///         namespace.
///
///////////////////////////////////////////////////////////
namespace transforms
{

/*! \addtogroup group_transforms Transforms
\brief The transform classes apply a transformation to
		one input image and return the result of the
		transformation into an output image.

Usually the transforms require that the input and the
 output images use the same color space, but the
 color transforms are able to copy the pixel data
 from the color space of the input image into the
 color space of the output image.

The application can call
 transforms::transform::allocateOutputImage() to
 allocate an output image that is compatible with the
 selected transform and input image.\n
For instance, once an image has been retrieved from
 a dataSet we can ask the modalityVOILUT transform
 to allocate an output image for us, and it will
 allocate an image with the right color space and
 bit depth;

\code
// loadedDataSet is a ptr<dataSet> previously loaded
// Here we get the first image in the dataSet
ptr<image> inputImage(loadedDataSet->getImage(0));

// We need to get the image's size because we have to
//  tell the transform on which area we want to apply
//  the transform (we want all the image area)
imbxUint32 width, height;
inputImage->getSize(&width, &height);

// Allocate the modality transform. The modality transform
//  gets the transformation parameters from the dataset
ptr<transforms::modalityVOILUT> modalityTransform(new transforms::modalityVOILUT(loadedDataSet));

// We ask the transform to allocate a proper output image
ptr<image> outputImage(modalityTransform->allocateOutputImage(inputImage, width, height));

// And now we run the transform
modalityTransform->runTransform(inputImage, 0, 0, width, height, outputImage, 0, 0);
\endcode

All the transforms but the modalityVOILUT can convert
 the result to the bit depth of the output image, so for
 instance the transform colorTransforms::YBRFULLToRGB
 can take a 16 bits per channel input image and
 write the result to a 8 bits per color channel output
 image.\n
modalityVOILUT cannot do this because its output has
 to conform to the value in the tag 0028,1054; the
 tag 0028,1054 specifies the units of the modality VOI-LUT
 transform. modalityVOILUT::allocateOutputImage() is able
 output image that can hold the result of the
 to allocate the modality transformation.

*/
/// @{

/// \brief This is the base class for the transforms.
///
/// A transform takes one input and one output image:
///  the output image is modified according to the
///  transform's type, input image's content and
///  transform's parameter.
///
///////////////////////////////////////////////////////////
class transform : public baseObject
{

public:
	/// \brief Returns true if the transform doesn't do
	///         anything.
	///
	/// It always return false, but it is overwritten in the
	///  transformsChain class.
	///
	/// @return false if the transform does something, or true
	///          if the transform doesn't do anything (e.g. an
	///          empty transformsChain object).
	///
	///////////////////////////////////////////////////////////
	virtual bool isEmpty();


	/// \brief Allocate an output image that is compatible with
	///         the transform given the specified input image.
	///
	/// @param pInputImage image that will be used as input
	///                     image in runTransform()
	/// @param width       the width of the output image,
	///                     in pixels
	/// @param height      the height of the output image,
	///                     in pixels
	/// @return an image suitable to be used as output image
	///          in runTransform()
	///
	///////////////////////////////////////////////////////////
	virtual ptr<image> allocateOutputImage(ptr<image> pInputImage, imbxUint32 width, imbxUint32 height) = 0;

	/// \brief Executes the transform.
	///
	/// @param inputImage    the input image for the transform
	/// @param inputTopLeftX the horizontal position of the
	///                       top left corner of the area to
	///                       process
	/// @param inputTopLeftY the vertical position of the top
	///                       left corner of the area to
	///                       process
	/// @param inputWidth    the width of the area to process
	/// @param inputHeight   the height of the area to process
	/// @param outputImage   the output image for the transform
	/// @param outputTopLeftX the horizontal position of the
	///                       top left corner of the output
	///                       area
	/// @param outputTopLeftY the vertical position of the top
	///                        left corner of the output area
	///
	///////////////////////////////////////////////////////////
	virtual void runTransform(
            const ptr<image>& inputImage,
            imbxUint32 inputTopLeftX, imbxUint32 inputTopLeftY, imbxUint32 inputWidth, imbxUint32 inputHeight,
            const ptr<image>& outputImage,
			imbxUint32 outputTopLeftX, imbxUint32 outputTopLeftY) = 0;

};


/// \brief This is the base class for transforms that use
///         templates.
///
/// Transforms derived from transformHandlers
///  have the macro DEFINE_RUN_TEMPLATE_TRANSFORM in
///  their class definition and implement the template
///  function templateTransform().
///
///////////////////////////////////////////////////////////
class transformHandlers: public transform
{
public:
	/// \brief Reimplemented from transform: calls the
	///         templated function templateTransform().
	///
	/// @param inputImage    the input image for the transform
	/// @param inputTopLeftX the horizontal position of the
	///                       top left corner of the area to
	///                       process
	/// @param inputTopLeftY the vertical position of the top
	///                       left corner of the area to
	///                       process
	/// @param inputWidth    the width of the area to process
	/// @param inputHeight   the height of the area to process
	/// @param outputImage   the output image for the transform
	/// @param outputTopLeftX the horizontal position of the
	///                       top left corner of the output
	///                       area
	/// @param outputTopLeftY the vertical position of the top
	///                        left corner of the output area
	///
	///////////////////////////////////////////////////////////
	void runTransform(
			const ptr<image>& inputImage,
			imbxUint32 inputTopLeftX, imbxUint32 inputTopLeftY, imbxUint32 inputWidth, imbxUint32 inputHeight,
			const ptr<image>& outputImage,
			imbxUint32 outputTopLeftX, imbxUint32 outputTopLeftY) override;

	/// \internal
	virtual void runTransformHandlers(
			ptr<handlers::dataHandlerNumericBase> inputHandler, imbxUint32 inputHandlerWidth, const std::wstring& inputHandlerColorSpace,
			ptr<palette> inputPalette,
			imbxInt32 inputHandlerMinValue, imbxUint32 inputHandlerNumValues,
			imbxInt32 inputTopLeftX, imbxInt32 inputTopLeftY, imbxInt32 inputWidth, imbxInt32 inputHeight,
			ptr<handlers::dataHandlerNumericBase> outputHandler, imbxInt32 outputHandlerWidth, const std::wstring& outputHandlerColorSpace,
			ptr<palette> outputPalette,
			imbxInt32 outputHandlerMinValue, imbxUint32 outputHandlerNumValues,
			imbxInt32 outputTopLeftX, imbxInt32 outputTopLeftY) = 0;

};


/// \brief Base class for the exceptions thrown by the
///         transforms.
///
///////////////////////////////////////////////////////////
class transformException: public std::runtime_error
{
public:
	/// \brief Constructor.
	///
	/// @param message the cause of the exception
	///
	///////////////////////////////////////////////////////////
	transformException(const std::string& message): std::runtime_error(message){}
};

/// @}

} // namespace transforms

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraTransform_5DB89BFD_F105_45e7_B9D9_3756AC93C821__INCLUDED_)
