#if !defined(imebraBitTransformTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraBitTransformTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include <QtTest/QtTest>
#include "../library/base/include/baseObject.h"

namespace puntoexe
{


namespace imebra
{

namespace transforms
{
	class transform;
}

namespace tests
{

class bitTransformTest: public QObject
{
    Q_OBJECT

private slots:
	void testBitTransform();
	void testEmptyTransformsChain();
	void testEmptyVOILUT();
	void testEmptyModalityVOILUT();

private:
	void testEmptyTransform(ptr<transforms::transform> pTransform);
};

} // namespace tests

} // namespace imebra

} // namespace puntoexe


#endif // #if !defined(imebraBitTransformTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
