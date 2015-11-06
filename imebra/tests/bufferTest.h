#if !defined(imebraBufferTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraBufferTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include <QtTest/QtTest>

namespace puntoexe
{

namespace imebra
{

namespace tests
{

class bufferTest: public QObject
{
    Q_OBJECT

private slots:
    void testDefaultType();
    void testReadWrite();
    void testOddLength();
};

} // namespace tests

} // namespace imebra

} // namespace puntoexe


#endif // #if !defined(imebraBufferTest_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
