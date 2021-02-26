#include <utility>
#include <iostream>

#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorMath.h"         //Needed for samples_1D.
#include "YgorString.h"       //Needed for GetFirstRegex(...)

#include "doctest/doctest.h"
#include "CPD_Rigid.h"
#include "CPD_Affine.h"
#include "CPD_Shared.h"
#include "CPD_Nonrigid.h"

/*
This unit testing uses the DocTest framework. This is the framework
used in the unit tests for the overall DICOMautomaton repository.

To run tests, run ./compile_and_run.sh from the terminal and a summary
of the tests passed and failed will be printed on the terminal.

For information on how to use this testing framework see:
https://github.com/onqtam/doctest
*/

// rigid test
TEST_CASE("GetA"){
	Eigen::MatrixXd xHat(4, 3);
	xHat << 4, 5, 6,
			7, 8, 9,
			1, 1, 1,
			2, 2, 2;

	Eigen::MatrixXd yHat(4, 3);
	yHat << 4, 5, 1,
			2, 4, 1,
			8, 8, 8,
			1, 4, 2;

	Eigen::MatrixXd postProb(4, 4);
	postProb << 1, 4, 1, 6,
				0, 1, 2, 1,
				1, 0, 2, 3,
				1, 8, 3, 0;

	Eigen::MatrixXd answerA(3,3);
	answerA << 	361, 617, 278,
				400, 690, 310,
				439, 763, 342;

	Eigen::MatrixXd A(3,3);
	A = GetA(xHat, yHat, postProb);

	REQUIRE(A.isApprox(answerA));
}
// rigid test
TEST_CASE("GetRotationMatrix"){
	Eigen::MatrixXd U(3, 3);
	U <<	1, 0, 1,
			1, 5, 1,
			2, 1, 3;

	Eigen::MatrixXd V(3, 3);
	V << 	2, 8, 1,
			3, 2, 1,
			1, 5, 6;

	Eigen::MatrixXd answerR(3,3);
	answerR << 	-543, -542, -3269,
				-503, -532, -3244,
				-1623, -1627, -9803;

	Eigen::MatrixXd R(3,3);
	R = GetRotationMatrix(U, V);

	REQUIRE(R.isApprox(answerR));
}
// rigid test
TEST_CASE("GetS"){
	Eigen::MatrixXd yHat(4, 3);
	yHat << 4, 5, 1,
			2, 4, 1,
			8, 8, 8,
			1, 4, 2;

	Eigen::MatrixXd postProb(4, 4);
	postProb << 1, 4, 1, 6,
				0, 1, 2, 1,
				1, 0, 2, 3,
				1, 8, 3, 0;

	Eigen::MatrixXd A(3,3);
	A << 	361, 617, 278,
			400, 690, 310,
			439, 763, 342;

	Eigen::MatrixXd R(3,3);
	R << 	-543, -542, -3269,
			-503, -532, -3244,
			-1623, -1627, -9803;

	double answerS = -4176.53765060241;
	double s = GetS(A, R, yHat, postProb);
	double threshold = 0.001;

	REQUIRE(answerS == doctest::Approx(s).epsilon(threshold));
}

// rigid test
TEST_CASE("SigmaSquared"){
	Eigen::MatrixXd xHat(4, 3);
	xHat << 4, 5, 6,
			7, 8, 9,
			1, 1, 1,
			2, 2, 2;

	Eigen::MatrixXd postProb(4, 4);
	postProb << 1, 4, 1, 6,
				0, 1, 2, 1,
				1, 0, 2, 3,
				1, 8, 3, 0;

	Eigen::MatrixXd A(3,3);
	A << 	361, 617, 278,
			400, 690, 310,
			439, 763, 342;

	Eigen::MatrixXd R(3,3);
	R << 	-543, -542, -3269,
			-503, -532, -3244,
			-1623, -1627, -9803;

	double s = -4176.53765060241;
	double answerSigmaSquared = -340660616.302194;
	double sigmasquared = SigmaSquared(s, A, R, xHat, postProb);
	double threshold = 0.001;

	REQUIRE(answerSigmaSquared == doctest::Approx(sigmasquared).epsilon(threshold));
}

TEST_CASE("B") {
	Eigen::MatrixXd xHat(3, 2);
	xHat << 3, 4,
			1, 1,
			1, 2;
	Eigen::MatrixXd yHat(3, 2);
	yHat << 1, 2,
			2, 4,
			1, 1;
	Eigen::MatrixXd postProb(3, 3);
	postProb << 1, 0, 1,
				2, 1, 1,
				1, 1, 1;
	Eigen::MatrixXd answerB(2, 2);
	answerB << (20./9), (-5./9),
				(28./9), (-7./9);
	Eigen::MatrixXd B;
	B = CalculateB(xHat, yHat, postProb);

	double threshold = 0.01;
	REQUIRE(((answerB - B).norm() < threshold));
}

TEST_CASE("sigma squared") {
	Eigen::MatrixXd xHat(3, 2);
	xHat << 3, 4,
			1, 1,
			1, 2;
	Eigen::MatrixXd yHat(3, 2);
	yHat << 1, 2,
			2, 4,
			1, 1;
	Eigen::MatrixXd postProb(3, 3);
	postProb << 1, 0, 1,
				2, 1, 1,
				1, 1, 1;
	Eigen::MatrixXd B(2, 2);
	B << 	(20./9), (-5./9),
			(28./9), (-7./9);
	
	double sigmaSquared = SigmaSquared(B, xHat, yHat, postProb);

	double sigmaSquaredAnswer = 257./162;
	double threshold = 0.001;

	REQUIRE(sigmaSquared == doctest::Approx(sigmaSquaredAnswer).epsilon(threshold));
}

TEST_CASE("init sigma squared") {
	Eigen::MatrixXd xPoints(3, 2);
	xPoints << 3, 4,
			1, 1,
			1, 2;
	Eigen::MatrixXd yPoints(3, 2);
	yPoints << 1, 2,
			2, 4,
			1, 1;
	double initSigmaSquared = Init_Sigma_Squared(xPoints, yPoints);
	double initSigmaSquaredAnswer = 39./18;
	double threshold = 0.001;

	REQUIRE(initSigmaSquared == doctest::Approx(initSigmaSquaredAnswer).epsilon(threshold));
}

TEST_CASE("E Step") {
	Eigen::MatrixXd xPoints(3, 2);
	xPoints  << 3, 4,
				1, 1,
				1, 2;
	Eigen::MatrixXd yPoints(3, 2);
	yPoints  << 1, 2,
				2, 4,
				1, 1;
	Eigen::MatrixXd B(2, 2);
	B    << 5, 2,
		 	1, 1;
	Eigen::MatrixXd t(2, 1);
	t << 	2,
			3;
	double w = 2./3;
	double sigmaSquared = 1.5;

	Eigen::MatrixXd postProbAnswer(3,3);
	postProbAnswer << 7.59784657e-12, 4.25691976e-20, 8.5502519e-19,
					  1.85585056e-47, 1.58360649e-63, 2.35028043e-61,
				      2.33560914e-07, 1.39159447e-13, 1.435048e-12;

	Eigen::MatrixXd postProb;
	postProb = E_Step(xPoints, yPoints, B, t, sigmaSquared, w, 1.0);
	double threshold = 0.001;

	for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
			REQUIRE(postProb(i,j) == doctest::Approx(postProbAnswer(i,j)).epsilon(threshold));
        }
    }
}

// nonrigid test
TEST_CASE("Gram Matrix") {
	Eigen::MatrixXd yPoints(4,3);
	yPoints << 4, 5, 1,
			2, 4, 1,
			8, 8, 8,
			1, 4, 2;
	double betaSquared = 2.0;

	Eigen::MatrixXd gram = GetGramMatrix(yPoints, betaSquared);
	Eigen::MatrixXd gramAnswer(4,4);
	gramAnswer << 1, 0.2865048, 9.237450e-9, 0.06392786,
				  0.2865058, 1, 1.08159416e-11, 0.6065307,
				  9.237450e-9, 1.08159416e-11, 1, 1.08159416e-11,
				  0.06392786, 0.6065307, 1.08159416e-11, 1;
	double threshold = 0.001;

	for (size_t i = 0; i < 4; ++i) {
		for (size_t j = 0; j < 4; ++j) {
			REQUIRE(gram(i,j) == doctest::Approx(gramAnswer(i,j)).epsilon(threshold));
		}
	}
}

TEST_CASE("E Step NONRIGID") {
	Eigen::MatrixXd xPoints(3, 3);
	xPoints  << 4, 5, 6,
				7, 8, 9,
				2, 2, 2;
	Eigen::MatrixXd yPoints(3, 3);
	yPoints  << 4, 5, 1,
				2, 4, 1,
				8, 8, 8;
	Eigen::MatrixXd G(3, 3);
	G    << 1, 0.2865048, 9.237450e-9,
			0.2865058, 1, 1.08159416e-11,
			9.237450e-9, 1.08159416e-11, 1;
	Eigen::MatrixXd W(3, 3);
	W << 	1, 4, 1,
			0, 2, 4,
			1, 3, 3;
	double w = 2./3;
	double sigmaSquared = 2;

	Eigen::MatrixXd postProbAnswer(3,3);
	postProbAnswer << 6.10999e-6, 4.23070e-7, 5.05435e-10,
					  0.00149771, 1.15206e-6, 9.84797e-7,
				      5.15431e-12, 0.000160079, 1.383951e-25;

	Eigen::MatrixXd postProb;
	postProb = E_Step_NR(xPoints, yPoints, G, W, sigmaSquared, w);
	double threshold = 0.001;

	for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
			// std::cout << "\n";
			// std::cout << i;
			// std::cout << "\n";
			// std::cout << j;
			// std::cout << "\n";
			// std::cout << postProb(i,j);
			// std::cout << "\n";
			// std::cout << postProbAnswer(i,j);
			// std::cout << "\n";
			REQUIRE(postProb(i,j) == doctest::Approx(postProbAnswer(i,j)).epsilon(threshold));
        }
    }
}

// TEST_CASE("Similarity rigid/affine") {
// 	Eigen::MatrixXd xPoints(4, 3);
// 	xPoints  << 4, 5, 6,
// 				7, 8, 9,
// 				1, 1, 1,
// 				2, 2, 2;
// 	Eigen::MatrixXd yPoints(4, 3);
// 	yPoints  << 4, 5, 1,
// 				2, 4, 1,
// 				8, 8, 8,
// 				1, 4, 2;
// 	Eigen::MatrixXd postProb(4, 4);
// 	postProb << 1, 4, 1, 6,
// 				2, 1, 2, 1,
// 				1, 0, 2, 3,
// 				1, 8, 3, 2;
// 	Eigen::MatrixXd rotationMat(3, 3);
// 	rotationMat <<  1, 4, 1,
// 					0, 2, 4,
// 					1, 3, 3;
// 	Eigen::MatrixXd translationVec(3, 1);
// 	translationVec << 	1, 
// 						2, 
// 						3;
// 	double sigmaSquared = 1.5;
// 	double scale = 2;

// 	double similarity = GetSimilarity(xPoints, yPoints, rotationMat, translationVec, scale);
// 	double similarityAns = 101601.445;

// 	double threshold = 0.001;
// 	REQUIRE(similarity == doctest::Approx(similarityAns).epsilon(threshold));

// }

// TEST_CASE("Similarity NONRIGID") {
// 	Eigen::MatrixXd xPoints(4, 3);
// 	xPoints  << 4, 5, 6,
// 				7, 8, 9,
// 				1, 1, 1,
// 				2, 2, 2;
// 	Eigen::MatrixXd yPoints(4, 3);
// 	yPoints  << 4, 5, 1,
// 				2, 4, 1,
// 				8, 8, 8,
// 				1, 4, 2;
// 	Eigen::MatrixXd postProb(4, 4);
// 	postProb << 1, 4, 1, 6,
// 				2, 1, 2, 1,
// 				1, 0, 2, 3,
// 				1, 8, 3, 2;
// 	Eigen::MatrixXd gramMat(4, 4);
// 	gramMat <<  0, 4, 3, 1,
// 				3, 3, 4, 1,
// 				0, 5, 3, 5,
// 				5, 3, 2, 4;
// 	Eigen::MatrixXd W(4, 3);
// 	W << 	2, 2, 4,
// 			4, 5, 4,
// 			4, 3, 5,
// 			3, 1, 5;

// 	double sigmaSquared = 1.5;

// 	double similarity = GetSimilarity_NR(xPoints, yPoints, gramMat, W);
// 	double similarityAns = 67348.1115;

// 	double threshold = 0.001;
// 	REQUIRE(similarity == doctest::Approx(similarityAns).epsilon(threshold));

// }


TEST_CASE("Update SS non-rigid") {
	Eigen::MatrixXd xPoints(4, 3);
	xPoints  << 4, 5, 6,
				7, 8, 9,
				1, 1, 1,
				2, 2, 2;
	Eigen::MatrixXd postProb(4, 4);
	postProb << 1, 2, 3, 4,
				5, 4, 3, 2,
				1, 1, 1, 1,
				2, 2, 2, 2;
	Eigen::MatrixXd transformedPoints(4, 3);
	transformedPoints << 1, 1, 1,
						 2, 2, 2,
						 1, 2, 3,
						 3, 2, 1;
	double sigmaSquared = SigmaSquared(xPoints, postProb, transformedPoints);

	double sigmaSquaredAnswer = 1412./108;
	double threshold = 0.001;

	REQUIRE(sigmaSquared == doctest::Approx(sigmaSquaredAnswer).epsilon(threshold));
}

TEST_CASE("CalculateUx") {
	Eigen::MatrixXd xPoints(3, 2);
	xPoints <<  3, 4,
				1, 1,
				1, 2;
	Eigen::MatrixXd postProb(3, 3);
	postProb << 1, 0, 1,
				2, 1, 1,
				1, 1, 1;
	Eigen::MatrixXd answerUx(2, 1);
	answerUx << (17./9),
				(24./9);

	Eigen::MatrixXd Ux;
	Ux = CalculateUx(xPoints, postProb);

	double threshold = 0.01;
	for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 1; ++j) {
			REQUIRE(Ux(i,j) == doctest::Approx(answerUx(i,j)).epsilon(threshold));
        }
    }
}

TEST_CASE("CalculateUy") {
	Eigen::MatrixXd yPoints(3, 2);
	yPoints <<  3, 4,
				1, 1,
				1, 2;
	Eigen::MatrixXd postProb(3, 3);
	postProb << 1, 0, 1,
				2, 1, 1,
				1, 1, 1;
	Eigen::MatrixXd answerUy(2, 1);
	answerUy << (13./9),
				(18./9);

	Eigen::MatrixXd Uy;
	Uy = CalculateUy(yPoints, postProb);

	double threshold = 0.01;
	for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 1; ++j) {
			REQUIRE(Uy(i,j) == doctest::Approx(answerUy(i,j)).epsilon(threshold));
        }
    }
}

TEST_CASE("AlignedPointSet NONRIGID") {
	Eigen::MatrixXd yPoints(4, 3);
	yPoints  << 4, 5, 1,
				2, 4, 1,
				8, 8, 8,
				1, 4, 2;
	Eigen::MatrixXd gramMat(4, 4);
	gramMat <<  0, 4, 3, 1,
				3, 3, 4, 1,
				0, 5, 3, 5,
				5, 3, 2, 4;
	Eigen::MatrixXd W(4, 3);
	W << 	2, 2, 4,
			4, 5, 4,
			4, 3, 5,
			3, 1, 5;

	Eigen::MatrixXd answerAligned(4, 3);
	answerAligned << 	35, 35, 37,
						39, 38, 50,
						55, 47, 68,
						43, 39, 64;

	Eigen::MatrixXd AlignedPS;
	AlignedPS = AlignedPointSet_NR(yPoints, gramMat, W);

	double threshold = 0.001;
	for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 3; ++j) {
			REQUIRE(AlignedPS(i,j) == doctest::Approx(answerAligned(i,j)).epsilon(threshold));
        }
    }
}
// non rigid test
TEST_CASE("GetW") {
	Eigen::MatrixXd yPoints(4, 3);
	yPoints <<   3, 32, 10,
    		    10,  6,  2,
    			10, 10, 43,
     			 1,  2, 21;

	Eigen::MatrixXd xPoints(4, 3);
	xPoints <<   4,  5,  7,
    			10, 23,  4,
     			 6,  7, 20,
    			34, 10,  2;

	Eigen::MatrixXd postProb(4, 4);
	postProb << 0.1, 0.4, 0.4, 0.7, 
				0.5, 0.7, 0.8, 0.8, 
				0.2, 0.2, 0.5, 0.2, 
				0.9, 0.2, 0.7, 0.7;

	Eigen::MatrixXd gramMatrix(4, 4);
	gramMatrix <<   1, 0.5, 0.8, 0.2, 
				0.5,   1, 0.1, 0.6, 
				0.8, 0.1,   1, 0.3, 
				0.2, 0.6, 0.3,   1;

	double sigmaSquared = 2;
	double lambda = 0.5;

	Eigen::MatrixXd answerW(4, 3);
	answerW << 13.4915220591003, -18.973075298976, 5.86045059959459, 
				-5.87043357937814, 9.84038392458258, 7.94248048306686, 
				-6.28462294910371, 7.2451892612769, -17.9879677078044,
				10.8209590146699, 1.51201994761614, -8.9579914981555;

	Eigen::MatrixXd W = GetW(xPoints, yPoints, gramMatrix, postProb, sigmaSquared, lambda);

	double threshold = 0.01;
	for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 3; ++j) {
			REQUIRE(W(i,j) == doctest::Approx(answerW(i,j)).epsilon(threshold));
        }
    }
}

// power iteration test
TEST_CASE("PowerIteration") {
	Eigen::MatrixXd m(3, 3);
	m <<   2, 2, 3,
    		     4, 5, 6,
    			 7, 8, 9;

	Eigen::VectorXd v = Eigen::VectorXd::Random(3);

	double ev = PowerIteration(m, v, 20, 0.01);
	double threshold = 0.01;

	REQUIRE((ev) == doctest::Approx(16.234).epsilon(threshold));
	REQUIRE((v[0]) == doctest::Approx(0.245).epsilon(threshold));
	REQUIRE((v[1]) == doctest::Approx(0.523).epsilon(threshold));
	REQUIRE((v[2]) == doctest::Approx(0.816).epsilon(threshold));
}

// eigen decomposition test
TEST_CASE("NLargestEigenvalues") {
	Eigen::MatrixXd m(3, 3);
	m <<   -4, 14, 0,
		   -5, 13, 0,
		   -1,  0, 2;

	FUNCINFO("HELLO")
	Eigen::MatrixXd vector_matrix = Eigen::MatrixXd::Zero(3, 2);
	Eigen::MatrixXd value_matrix = Eigen::MatrixXd::Zero(2, 2);

	GetNLargestEigenvalues(m, vector_matrix, value_matrix, 2, 3, 20, 0.01);

	double threshold = 0.01;

	// REQUIRE((ev) == doctest::Approx(16.234).epsilon(threshold));
	// REQUIRE((v[0]) == doctest::Approx(0.245).epsilon(threshold));
	// REQUIRE((v[1]) == doctest::Approx(0.523).epsilon(threshold));
	// REQUIRE((v[2]) == doctest::Approx(0.816).epsilon(threshold));
}
