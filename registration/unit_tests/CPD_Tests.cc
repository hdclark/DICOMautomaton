#include <utility>
#include <iostream>
#include "doctest/doctest.h"
#include "CPD_Rigid.h"
#ifndef CPDAFFINE_H_
#include "CPD_Affine.h"
#define CPDAFFINE_H_
#endif
#ifndef CPDSHARED_H_
#define CPDSHARED_H_
#include "CPD_Shared.h"
#endif


/*
This unit testing uses the DocTest framework. This is the framework
used in the unit tests for the overall DICOMautomaton repository.

To run tests, run ./compile_and_run.sh from the terminal and a summary
of the tests passed and failed will be printed on the terminal.

For information on how to use this testing framework see:
https://github.com/onqtam/doctest
*/

TEST_CASE("example test"){
	REQUIRE(1==1);

	SUBCASE("example subcase"){
		REQUIRE(2==2);
	}
}

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
	double threshold = 0.01;

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
	double threshold = 0.01;

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

	for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
			REQUIRE(postProb(i,j) == doctest::Approx(postProbAnswer(i,j)));
        }
    }
}