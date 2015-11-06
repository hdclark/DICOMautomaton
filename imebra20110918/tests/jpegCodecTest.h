#if !defined(imebraJpegCodecTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraJpegCodecTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include <QtTest/QtTest>

namespace puntoexe
{

namespace imebra
{

namespace tests
{

class jpegCodecTest: public QObject
{
	Q_OBJECT

private slots:
	void testBaseline();
	void testBaselineSubsampled();
	void testLossless();
};

} // namespace tests

} // namespace imebra

} // namespace puntoexe


#endif // #if !defined(imebraJpegCodecTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
