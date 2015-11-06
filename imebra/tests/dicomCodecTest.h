#if !defined(imebraDicomCodecTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraDicomCodecTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include <QtTest/QtTest>
#include "../library/imebra/include/image.h"

namespace puntoexe
{

namespace imebra
{

namespace tests
{

class dicomCodecTest: public QObject
{
	Q_OBJECT

private slots:
	void testUncompressed();
	void testRLENotInterleaved();

protected:
	ptr<image> makeTestImage();
};

} // namespace tests

} // namespace imebra

} // namespace puntoexe


#endif // #if !defined(imebraDicomCodecTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
