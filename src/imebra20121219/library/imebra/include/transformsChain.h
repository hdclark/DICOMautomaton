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

/*! \file transformsChain.h
    \brief Declaration of the class transformsChain.

*/

#if !defined(imebraTransformsChain_5DB89BFD_F105_45e7_B9D9_3756AC93C821__INCLUDED_)
#define imebraTransformsChain_5DB89BFD_F105_45e7_B9D9_3756AC93C821__INCLUDED_

#include <list>
#include "transform.h"

namespace puntoexe
{

namespace imebra
{

namespace transforms
{

/// \addtogroup group_transforms
///
/// @{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Executes a sequence of transforms.
///
/// Before calling doTransform, specify the sequence
///  by calling addTransform().
/// Each specified transforms take the output of the 
///  previous transform as input.
///
/// The first defined transform takes the input images
///  defined in the transformsChain object, the last
///  defined transforms uses the output images defined
///  in the transformsChain object.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class transformsChain: public transform
{
public:
	transformsChain();

	/// \brief Add a transform to the transforms chain.
	///
	/// The added transform will take the output of the 
	///  previously added transform as an input image and will
	///  supply its output to the next added transform or as
	///  an output of the transformsChain if it is the
	///  last added transform.
	///
	/// @param pTransform the transform to be added to
	///                    transformsChain
	///
	///////////////////////////////////////////////////////////
	void addTransform(ptr<transform> pTransform);

	virtual void runTransform(
            const ptr<image>& inputImage,
            imbxUint32 inputTopLeftX, imbxUint32 inputTopLeftY, imbxUint32 inputWidth, imbxUint32 inputHeight,
            const ptr<image>& outputImage,
            imbxUint32 outputTopLeftX, imbxUint32 outputTopLeftY);

	/// \brief Returns true if the transform doesn't do
	///         anything.
	///
	/// @return false if the transform does something, or true
	///          if the transform doesn't do anything (e.g. an
	///          empty transformsChain object).
	///
	///////////////////////////////////////////////////////////
	virtual bool isEmpty();

	virtual ptr<image> allocateOutputImage(ptr<image> pInputImage, imbxUint32 width, imbxUint32 height);

protected:
	imbxUint32 m_inputWidth;
	imbxUint32 m_inputHeight;
	std::wstring m_inputColorSpace;
	image::bitDepth m_inputDepth;
	imbxUint32 m_inputHighBit;
	std::wstring m_outputColorSpace;
	image::bitDepth m_outputDepth;
	imbxUint32 m_outputHighBit;

	typedef std::list<ptr<transform> > tTransformsList;
	tTransformsList m_transformsList;

	typedef std::list<ptr<image> > tTemporaryImagesList;
	tTemporaryImagesList m_temporaryImages;

};

class transformsChainException: public transformException
{
public:
	transformsChainException(const std::string& what): transformException(what){}
};

/// @}

} // namespace transforms

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraTransformsChain_5DB89BFD_F105_45e7_B9D9_3756AC93C821__INCLUDED_)
