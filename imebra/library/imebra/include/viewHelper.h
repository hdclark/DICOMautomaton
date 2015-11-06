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

/*! \file viewHelper.h
    \brief Declaration of the a class that help a view or window class to draw
            a dicom image on the screen.

This file is not included automatically by imebra.h

*/

#if !defined(imebraView_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
#define imebraView_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_

#include "../../base/include/baseObject.h"
#include "image.h"
#include "dataSet.h"
#include "drawBitmap.h"
#include <list>
//#include <memory>

namespace puntoexe
{

namespace imebra
{

/// \addtogroup group_helpers
///
/// @{


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \internal
/// \brief This class saves the properties of one cursor's
///        line.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class cursorLine: public baseObject
{
public:
	imbxInt32 m_x0;
	imbxInt32 m_y0;
	imbxInt32 m_x1;
	imbxInt32 m_y1;

	imbxUint8 m_red;
	imbxUint8 m_green;
	imbxUint8 m_blue;

	enum tStyle
	{
		line,
		dotted
	} m_style;
	imbxUint32 m_width;

public:
	cursorLine();
	cursorLine(
		imbxInt32 x0,
		imbxInt32 y0,
		imbxInt32 x1,
		imbxInt32 y1,
		imbxUint8 red,
		imbxUint8 green,
		imbxUint8 blue,
		tStyle style,
		imbxUint32 width);

	cursorLine(const cursorLine& right);
	cursorLine& operator=(const cursorLine& right);

protected:
	void copy(const cursorLine& right);

};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This class can be used as a base class by
///         the windows that need to display a Dicom image.
///
/// The is an abstract class, so it cannot be used alone;
/// it provides some functions that the derived class must
///  always overwrite because they are platform specific.
///
/// Because this class must always be overwritten its
///  header is not included in the file imebra.h and
///  should be included in the header file of the
///  derived class.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class view
{
public:
	/// \brief Costructor.
	///
	/// Each time a redraw operation is needed this class
	///  generates a bitmap that is then passed to the
	///  virtual function drawBitmap().
	///
	/// The derived class can specify the byte alignment for
	///  each bitmap's row and the order of the RGB components.
	///
	/// @param rowByteAlign   the number of bytes each bitmap's
	///                        row has to be aligned to.
	///                       Usually is for bytes on Windows
	/// @param bBGR           true if the RGB components in
	///                        the bitmap has to be blue, green
	///                        and then red. false if the
	///                        order has to be red, green, blue
	///
	///////////////////////////////////////////////////////////
	view(imbxInt32 rowByteAlign, bool bBGR);

	///////////////////////////////////////////////////////////
	/// \name Get/Set the image to be show
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Define the image that the window must display
	///
	/// After the image has been set the function will
	///  invalidate the window and the operating framework
	///  will take care of sending a repaint message to it.
	///
	/// If the view already had an image set with setImage()
	///  then the function will take one of the following
	///  actions:
	///  - if the new image has the same size of the old one
	///     (in pixels and millimiters), then the new image
	///     will be displayed in the same position of the old
	///     one
	///  - if the new image's size differs from the old one
	///     then a new position will be calculated for the
	///     new image so it will fit completly in the window's
	///     client area
	///
	/// @param pImage       the image to be displayed in the
	///                      window, or a null pointer to
	///                      remove the image from the window
	/// @param pChain       the chain of transforms to be
        ///                      applied to the image before it is
        ///                      displayed. Can be null
	///
	///////////////////////////////////////////////////////////
	void setImage(ptr<image> pImage, ptr<transforms::transformsChain> pChain);

	/// \brief Retrieve the image currently displayed in the
	///         window
	///
	/// @return the image currently displayed in the window
	///
	///////////////////////////////////////////////////////////
	ptr<image> getImage();

	//@}


	///////////////////////////////////////////////////////////
	/// \name Get/Set the background color
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Get the  color used to draw the background
	///         around the image.
	///
	/// @param pRed   a pointer to a variable filled with the
	///                red component of the background
	/// @param pGreen a pointer to a variable filled with the
	///                green component of the background
	/// @param pBlue  a pointer to a variable filled with the
	///                blue component of the background
	///
	///////////////////////////////////////////////////////////
	void getBackgroundColor(imbxUint8* pRed, imbxUint8* pGreen, imbxUint8* pBlue);

	/// \brief Set the color to be used to draw the
	///         background around the image.
	///
	/// @param red   the red component of the background color
	/// @param green the green component of the background
	///               color
	/// @param blue  the blue component of the background color
	///
	///////////////////////////////////////////////////////////
	void setBackgroundColor(imbxUint8 red, imbxUint8 green, imbxUint8 blue);

	//@}


	///////////////////////////////////////////////////////////
	/// \name Zoom and image's position
	///
	///////////////////////////////////////////////////////////
	//@{
public:
	/// \brief Retrieve the coordinates of the rectangle that
	///         contains the image.
	///
	/// When an image is set with setImage() or the zoom factor
	///  is changed, the rectangle containing the image is
	///  recalculated by updateImageRect().
	///
	/// This function retrieves the latest rectangle calculated
	///  by updateImageRect().
	///
	/// @param pLeft    a pointer to a variable filled with the
	///                  horizontal coordinate of the
	///                  rectangle's top-left corner
	/// @param pTop     a pointer to a variable filled with the
	///                  vertical coordinate of the rectangle's
	///                  top-left corner
	/// @param pRight   a pointer to a variable filled with the
	///                  horizontal coordinate of the
	///                  rectangle's bottom-right corner
	/// @param pBottom  a pointer to a variable filled with the
	///                  vertical coordinate of the rectangle's
	///                  bottom-right corner
	///
	///////////////////////////////////////////////////////////
	void getImageRect(imbxInt32* pLeft, imbxInt32* pTop, imbxInt32* pRight, imbxInt32* pBottom);

	/// \brief Retrieve the active zoom factor.
	///
	/// The zoom factor is used to calculate the rectangle in
	///  which the image is drawn.
	///
	/// The zoom factor can be set by one of the following
	///  functions:
	/// - setZoomFactor()
	/// - setZoomRect()
	/// - zoomInOut()
	///
	/// Even when the zoom factor has been set to an automatic
	///  value (-1) this function retrieve the real zoom
	///  factor.
	///
	/// @return the active zoom factor.
	///
	///////////////////////////////////////////////////////////
	double getZoomFactor();

	/// \brief Set the active zoom factor.
	///
	/// The active zoom factor uses the display's size and the
	///  image's size (in millimeters) to calculate the
	///  rectangle in which the image is displayed.
	///
	/// When the zoom factor is 1 then the image's size on the
	///  display will be a one-to-one representation of the
	///  real image.
	///
	/// A negative zoom factor enables the automatic zoom which
	///  displays the whole image in the window.
	/// When the zoom factor is in automatic mode then the
	///  size of the displayed image changes if the window's
	///  size is modified.
	///
	/// This function calls updateImageRect() and updates the
	///  window.
	///
	/// @param zoomFactor       the zoom factor to use when
	///                          drawing the image
	/// @param centerPointX     the horizontal coordinate of
	///                          the image's pixel that should
	///                          be centered in the window's
	///                          area. If -1 then the function
	///                          calculates its value
	///                          automatically
	/// @param centerPointY     the vertical coordinate of the
	///                          image's pixek that should be
	///                          centered in the window's area.
	///                          If -1 then the function
	///                          calculates its value
	///                          automatically
	///
	///////////////////////////////////////////////////////////
	void setZoomFactor(double zoomFactor, imbxInt32 centerPointX=-1, imbxInt32 centerPointY=-1);

	/// \brief Zoom to the selected image's area.
	///        The rectangle's coordinates are in image's
	///         pixels.
	///
	/// This function calls updateImageRect() and updates the
	///  window.
	///
	/// @param left    the horizontal coordinate of the
	///                 top-left corner of the image's
	///                 rectangle that must be shown in the
	///                 window
	/// @param top     the vertical coordinate of the top-left
	///                 corner of the image's rectangle that
	///                 must be shown in the window
	/// @param right   the horizontal coordinate of the
	///                 bottom-right corner of the image's
	///                 rectangle that must be shown in the
	///                 window
	/// @param bottom  the vertical coordinate of the
	///                 bottom-right corner of the image's
	///                 rectangle that must be shown in the
	///                 window
	///
	///////////////////////////////////////////////////////////
	void setZoomRect(imbxInt32 left, imbxInt32 top, imbxInt32 right, imbxInt32 bottom);

	/// \brief Increase or decrease the zoom factor.
	///
	/// This function calls updateImageRect() and updates the
	///  window.
	///
	/// @param bZoomIn      when true the function increase the
	///                      zoom factor, otherwise it
	///                      decreases the zoom factor
	/// @param centerPointX the horizontal coordinate of the
	///                      image's pixel that must be
	///                      centered in the window's area
	/// @param centerPointY the vertical coordinate of the
	///                      image's pixel that must be
	///                      centered in the window's area
	///
	///////////////////////////////////////////////////////////
	void zoomInOut(bool bZoomIn=true, imbxInt32 centerPointX=-1, imbxInt32 centerPointY=-1);

	/// \brief Return true if the zoom is in automatic mode.
	///        The zoom can be set in automatic mode by
	///         calling setZoomFactor() with a negative zoom
	///         factor.
	///
	/// When the zoom is in automatic mode then the size of
	///  the rectangle that contains the image is modified
	///  when the window's size is modified.
	///
	/// @return true if the zoom is in automatic mode, false
	///          otherwise
	///
	///////////////////////////////////////////////////////////
	bool isAutoZoom();

	void getCenterPoint(imbxInt32* pCenterPointX, imbxInt32* pCenterPointY);
	void setCenterPoint(imbxInt32 centerPointX, imbxInt32 centerPointY);

	//@}


	///////////////////////////////////////////////////////////
	//
	// Cursor
	//
	///////////////////////////////////////////////////////////
public:
	// Begin the cursor definition
	///////////////////////////////////////////////////////////
	void startCursorDef();

	/// End the cursor definition and make the hotspot visible
	///
	/// @param cursorHotSpotX the current horizontal position 
	///                        of the mouse, in image's pixels
	/// @param cursorHotSpotY the current vertical position of
	///                        the mouse, in image's pixels
	///          
	///////////////////////////////////////////////////////////
	void endCursorDef(imbxInt32 cursorHotSpotX, imbxInt32 cursorHotSpotY);

	// End the cursor definition
	///////////////////////////////////////////////////////////
	void endCursorDef();

	// Define a cursor's line
	///////////////////////////////////////////////////////////
	void defCursorLine(imbxInt32 startPointX, imbxInt32 startPointY, imbxInt32 endPointX, imbxInt32 endPointY, imbxUint8 colorRed, imbxUint8 colorGreen, imbxUint8 colorBlue, cursorLine::tStyle style = cursorLine::line, int width=0);

	/// \brief Translate the window's horizontal coordinate
	///         into the image's horizontal coordinate.
	///
	/// Use in pair with windowPosToImageY to translate the
	///  horizontal and the vertical coordinates.
	///
	/// The window's coordinates must include any scroll
	///  adjustment.
	///
	/// @param windowPosX  the horizontal window's coordinate,
	///                     adjusted with the current
	///                     horizontal scroll position
	/// @return            the image's horizontal coordinate.
	///                    The returned is always adjusted to
	///                     the valid range (is never less than
	///                     0 and never more than the image's
	///                     horizontal size)
	///
	///////////////////////////////////////////////////////////
	imbxInt32 windowPosToImageX(imbxInt32 windowPosX);

	/// \brief Translate the window's vertical coordinate into
	///         the image's vertical coordinate.
	///
	/// Use in pair with windowPosToImageX to translate the
	///  horizontal and the vertical coordinates.
	///
	/// The window's coordinates must include any scroll adjustment.
	///
	/// @param windowPosY  the vertical window's coordinate,
	///                     adjusted with the current vertical
	///                     scroll position
	/// @return            the image's vertical coordinate.
	///                    The returned is always adjusted to the
	///                     valid range (is never less than 0 and
	///                     never more than the image's vertical
	///                     size)
	///
	///////////////////////////////////////////////////////////
	imbxInt32 windowPosToImageY(imbxInt32 windowPosY);

	/// \brief Return a pixel's horizontal position in
	///         millimeters.
	///
	/// @param imagePosX   the horizontal pixel position for
	///                     which the position in millimeters
	///                     has to be retrieved
	/// @return the horizontal position of the specified pixel,
	///          in millimeters
	///
	///////////////////////////////////////////////////////////
	double imagePosToMillimitersX(imbxInt32 imagePosX);

	/// \brief Return a pixel's vertical position in
	///         millimeters.
	///
	/// @param imagePosY   the vertical pixel position for
	///                     which the position in millimeters
	///                     has to be retrieved
	/// @return the vertical position of the specified pixel,
	///          in millimeters
	///
	///////////////////////////////////////////////////////////
	double imagePosToMillimitersY(imbxInt32 imagePosY);

	/// \brief Return the horizontal pixel's index that
	///         lays at the specified horizontal position, in
	///         millimeters
	///
	/// @param millimitersX the horizontal position in
	///                      millimiters
	/// @return the index of the pixel that lays at the
	///          specified position
	///
	///////////////////////////////////////////////////////////
	imbxInt32 millimitersToImagePosX(double millimitersX);

	/// \brief Return the vertical pixel's index that
	///         lays at the specified vertical position, in
	///         millimeters
	///
	/// @param millimitersY the vertical position in
	///                      millimiters
	/// @return the index of the pixel that lays at the
	///          specified position
	///
	///////////////////////////////////////////////////////////
	imbxInt32 millimitersToImagePosY(double millimitersY);

	// This function must be called by your window class after
	//  the window's size has been updated or when some image's
	//  parameters have been changed
	//
	///////////////////////////////////////////////////////////
	void updateImageRect(imbxInt32 centerPointX, imbxInt32 centerPointY);

	/// \brief Draw the image.
	///
	/// @param pDeviceContext   the pointer or the handle of
	///                          the device context that
	///                          must be used. This value is
	///                          passed directly to the
	///                          functions called by this one:
	///                          - drawRectangle()
	///                          - drawBitmap()
	///                          - drawCursorLine()
	/// @param left             the left side of the visible
	///                          rectangle
	/// @param top              the top side of the visible
	///                          rectangle
	/// @param right            the right side of the visible
	///                          rectangle
	/// @param bottom           the bottom side of the visible
	///                          rectangle
	///
	///////////////////////////////////////////////////////////
	template <tDrawBitmapType drawBitmapType, int rowAlignBytes>
	void draw(void* pDeviceContext, imbxInt32 left, imbxInt32 top, imbxInt32 right, imbxInt32 bottom)
        {
            // Check if the image is visible in the specified area
            ///////////////////////////////////////////////////////////
            if(m_originalImage == 0 ||
                    left >= m_rightPosition ||
                    top >= m_bottomPosition ||
                    right <= m_leftPosition ||
                    bottom <= m_topPosition)
            {
                    drawRectangle(pDeviceContext, left, top, right, bottom, m_backgroundRed, m_backgroundGreen, m_backgroundBlue);
                    return;
            }

            if(left==right || top==bottom)
            {
                    return;
            }

            // Adjust the area to be drawn
            ///////////////////////////////////////////////////////////
            imbxInt32 bitmapLeft   = left;
            imbxInt32 bitmapTop    = top;
            imbxInt32 bitmapRight  = right;
            imbxInt32 bitmapBottom = bottom;

            if(bitmapLeft < m_leftPosition) bitmapLeft = m_leftPosition;
            if(bitmapTop < m_topPosition) bitmapTop = m_topPosition;
            if(bitmapRight > m_rightPosition) bitmapRight = m_rightPosition;
            if(bitmapBottom > m_bottomPosition) bitmapBottom = m_bottomPosition;


            if(bitmapRight > bitmapLeft && bitmapBottom > bitmapTop)
            {
                imbxUint32 bitmapRowLength = ((3 * (bitmapRight - bitmapLeft) + rowAlignBytes - 1) / rowAlignBytes) * rowAlignBytes;
				if(
					m_bUpdateImage ||
					m_cachedWidth != (m_rightPosition - m_leftPosition) ||
					m_cachedHeight != (m_bottomPosition - m_topPosition) ||
					m_cachedVisibleLeft != (bitmapLeft - m_leftPosition) ||
					m_cachedVisibleTop != (bitmapTop - m_topPosition) ||
					m_cachedVisibleRight != (bitmapRight - m_leftPosition) ||
					m_cachedVisibleBottom != (bitmapBottom - m_topPosition) ||
					m_bitmapMemory == 0)
				{
					m_cachedWidth = m_rightPosition - m_leftPosition;
					m_cachedHeight = m_bottomPosition - m_topPosition;
					m_cachedVisibleLeft = bitmapLeft - m_leftPosition;
					m_cachedVisibleTop = bitmapTop - m_topPosition;
					m_cachedVisibleRight = bitmapRight - m_leftPosition;
					m_cachedVisibleBottom = bitmapBottom - m_topPosition;
					m_bUpdateImage = false;
					m_bitmapMemory = m_drawBitmap->getBitmap<drawBitmapType, rowAlignBytes>(m_cachedWidth, m_cachedHeight,
                        m_cachedVisibleLeft, m_cachedVisibleTop, m_cachedVisibleRight, m_cachedVisibleBottom, m_bitmapMemory);
				}
                drawBitmap(pDeviceContext, bitmapLeft, bitmapTop, bitmapRight, bitmapBottom, bitmapRowLength, m_bitmapMemory->data());
            }

            if(top < m_topPosition)
            {
                    drawRectangle(pDeviceContext, left, top, right, m_topPosition, m_backgroundRed, m_backgroundGreen, m_backgroundBlue);
            }

            if(bottom > m_bottomPosition)
            {
                    drawRectangle(pDeviceContext, left, m_bottomPosition, right, bottom, m_backgroundRed, m_backgroundGreen, m_backgroundBlue);
            }

            if(left < m_leftPosition)
            {
                    drawRectangle(pDeviceContext, left, top, m_leftPosition, bottom, m_backgroundRed, m_backgroundGreen, m_backgroundBlue);
            }

            if(right > m_rightPosition)
            {
                    drawRectangle(pDeviceContext, m_rightPosition, top, right, bottom, m_backgroundRed, m_backgroundGreen, m_backgroundBlue);
            }

        }

	void drawCursor(void* pDeviceContext);


	///////////////////////////////////////////////////////////
	/// \name Pure virtual functions
	///
	///////////////////////////////////////////////////////////
	//@{
public:
	/// \brief Overwrite this method with a function that
	///         retrieve the size of the window's client area,
	///         in screen's pixels.
	///
	/// @param pSizeX    a pointer to the value that the
	///                   function fills with the window's
	///                   width, in pixels
	/// @param pSizeY    a pointer to the value that the
	///                   function fills with the window's
	///                   height, in pixels
	///
	///////////////////////////////////////////////////////////
	virtual void getWindowSize(imbxUint32* pSizeX, imbxUint32* pSizeY) = 0;

	/// \brief Overwrite this method with a function that
	///         retrieves the total virtual size of the window.
	///
	/// The virtual size is the size managed by the window's
	///  scroll bars, in screen's pixels.
	///
	/// @param pScrollSizeX a pointer to the value that the
	///                      function fills with the window's
	///                      virtual width, in pixels
	/// @param pScrollSizeY a pointer to the value that the
	///                      function fills with the window's
	///                      virtual height, in pixels
	///
	///////////////////////////////////////////////////////////
	virtual void getScrollSize(imbxUint32* pScrollSizeX, imbxUint32* pScrollSizeY) = 0;

	/// \brief Overwrite this method with a function that sets
	///         the window's virtual size.
	///
	/// The virtual size is the size managed by the window's
	///  scroll bars, in screen's pixels.
	///
	/// @param newScrollSizeX  the virtual window's width that
	///                         must be set
	/// @param newScrollSizeY  the virtual window's height that
	///                         must be set
	/// @param bInvalidate     is true if the function must
	///                         invalidate the window after
	///                         changing the virtual size,
	///                         false otherwise.
	///                         Note that the background
	///                          doesn't need to be invalidated
	///
	///////////////////////////////////////////////////////////
	virtual void setScrollSize(imbxUint32 newScrollSizeX, imbxUint32 newScrollSizeY, bool bInvalidate) = 0;

	/// \brief Overwrite this method with a function that
	///         retrieves the current position in the scroll
	///         bars, in screen's pixels.
	///
	/// @param pScrollX    a pointer to a variable that the
	///                     function must fill with the
	///                     horizontal scroll bar's position
	/// @param pScrollY    a pointer to a variable that the
	///                     function must fill with the
	///                     vertical scroll bar's position
	///
	///////////////////////////////////////////////////////////
	virtual void getScrollPosition(imbxInt32* pScrollX, imbxInt32* pScrollY) = 0;

	/// \brief Overwrite this method with a function that
	///         changes the position in the scroll bars, in
	///         screen's pixels.
	///
	/// @param scrollX  the new position for the horizontal
	///                  scroll bar, in pixels
	/// @param scrollY  the new position for the vertical
	///                  scroll bar, in pixels
	///
	///////////////////////////////////////////////////////////
	virtual void setScrollPosition(imbxInt32 scrollX, imbxInt32 scrollY)=0;

	/// \brief Overwrite this method with a function that
	///         draws a rectangle on a device context.
	///
	/// @param pDeviceContext a pointer to a device context
	///                        class. Is the same pointer
	///                        that your application passes
	///                        to the draw() or the
	///                        drawCursor() functions.
	/// @param left           the horizontal coordinate of the
	///                        rectangle's top-left corner
	/// @param top            the vertical coordinate of the
	///                        rectangle's top-left corner
	/// @param right          the horizontal coordinate of the
	///                        rectangle's bottom-right corner
	/// @param bottom         the vertical coordinate of the
	///                        rectangle's bottom-right corner
	/// @param colorRed       the red component of the
	///                        rectangle's color
	/// @param colorGreen     the green component of the
	///                        rectangle's color
	/// @param colorBlue      the blue component of the
	///                        rectangle's color
	///
	///////////////////////////////////////////////////////////
	virtual void drawRectangle(void* pDeviceContext, imbxInt32 left, imbxInt32 top, imbxInt32 right, imbxInt32 bottom, imbxUint8 colorRed, imbxUint8 colorGreen, imbxUint8 colorBlue)=0;

	/// \brief Overwrite this method with a function that
	///         draws a bitmap on the screen in the specified
	///         window's rectangle.
	///
	/// @param pDeviceContext the device context to use for
	///                        the drawing. This is the same
	///                        value that your application
	///                        passes to the draw() function
	/// @param left           the horizontal coordinate of the
	///                        top-left corner of the rectangle
	///                        that contains the bitmap
	/// @param top            the vertical coordinate of the
	///                        top-left corner of the rectangle
	///                        that contains the bitmap
	/// @param right          the horizontal coordinate of the
	///                        bottom-right corner of the
	///                        rectangle that contains the
	///                        bitmap
	/// @param bottom         the vertical coordinate of the
	///                        bottom-right corner of the
	///                        rectangle that contains the
	///                        bitmap
	/// @param bufferRowSizeBytes the size of a bitmap's row,
	///                        in bytes
	/// @param pBuffer        a pointer to an array if bytes
	///                        representing the bitmap.
	///
	///////////////////////////////////////////////////////////
	virtual void drawBitmap(void* pDeviceContext, imbxInt32 left, imbxInt32 top, imbxInt32 right, imbxInt32 bottom, imbxUint32 bufferRowSizeBytes, imbxUint8* pBuffer)=0;

	/// \brief Overwrite this method with a function that
	///         draws a line on the specified device context.
	///
	/// @param pDeviceContext the device context to use for
	///                        the drawing. This is the same
	///                        value that your application
	///                        passes to the drawCursor()
	///                        function
	/// @param startPointX    the horizontal coordinate of the
	///                        line's starting point, in
	///                        pixels
	/// @param startPointY    the vertical coordinate of the
	///                        line's starting point, in
	///                        pixels
	/// @param endPointX      the horizontal coordinate of the
	///                        line's ending point, in
	///                        pixels
	/// @param endPointY      the vertical coordinate of the
	///                        line's ending point, in
	///                        pixels
	/// @param colorRed       the red component of the
	///                        line's color
	/// @param colorGreen     the green component of the
	///                        line's color
	/// @param colorBlue      the blue component of the
	///                        line's color
	/// @param style          the line's style. It can be one
	///                        of the following values:
	///                        - cursorLine::line
	///                        - cursorLine::dotted
	/// @param width          the line's width, in pixels.
	///                       0 means really thin.
	///
	///////////////////////////////////////////////////////////
	virtual void drawCursorLine(void* pDeviceContext, imbxInt32 startPointX, imbxInt32 startPointY, imbxInt32 endPointX, imbxInt32 endPointY, imbxUint8 colorRed, imbxUint8 colorGreen, imbxUint8 colorBlue, cursorLine::tStyle style, imbxUint32 width)=0;

	/// \brief Overwrite this method with a function that
	///         invalidate the window, without
	///         redrawing it immediatly. The code doesn't
	///         have to invalidate the background.
	///
	///////////////////////////////////////////////////////////
	virtual void invalidate()=0;

	/// \brief Overwrite this method with a function that
	///         launches a redraw of the invalidated areas of
	///         the window.
	///
	///////////////////////////////////////////////////////////
	virtual void updateWindow()=0;

	/// \brief Overwrite this method with a function that
	///         retrieve the screen's resolution, in pixels
	///         per inch.
	///
	/// @param pHorzDPI a pointer to a variable that the
	///                  function must fill with the
	///                  horizontal resolution, in pixels
	///                  per inch
	/// @param pVertDPI a pointer to a variable that the
	///                  function must fill with the
	///                  vertical resolution, in pixels
	///                  per inch
	///
	///////////////////////////////////////////////////////////
	virtual void getScreenDPI(imbxUint32* pHorzDPI, imbxUint32* pVertDPI)=0;

	virtual bool isMouseCaptured()=0;

	//@}


private:
	typedef std::list<cursorLine> tCursorLinesList;

	imbxInt32 m_bitmapAlign;

        bool m_bBGR;

        // Image's position
	///////////////////////////////////////////////////////////
	imbxInt32 m_leftPosition;
	imbxInt32 m_topPosition;
	imbxInt32 m_rightPosition;
	imbxInt32 m_bottomPosition;

        // Background color
	///////////////////////////////////////////////////////////
	imbxUint8 m_backgroundRed;
	imbxUint8 m_backgroundGreen;
	imbxUint8 m_backgroundBlue;

	// Active image's zoom
	///////////////////////////////////////////////////////////
	double m_zoom;

        ptr<imebra::drawBitmap> m_drawBitmap;

	ptr<image> m_originalImage;

        ptr<memory> m_bitmapMemory;

	// Values used to retrieve the last bitmap
		bool m_bUpdateImage;
	imbxUint32 m_cachedWidth;
	imbxUint32 m_cachedHeight;
        imbxInt32 m_cachedVisibleTop;
        imbxInt32 m_cachedVisibleLeft;
        imbxInt32 m_cachedVisibleBottom;
        imbxInt32 m_cachedVisibleRight;



	// Cursor lines
	///////////////////////////////////////////////////////////
	tCursorLinesList m_cursorLines;

	// Temporary cursor lines
	///////////////////////////////////////////////////////////
	tCursorLinesList m_tempCursorLines;

};

/// @}

} // namespace imebra

} // namespace puntoexe

#endif // imebraView_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_
