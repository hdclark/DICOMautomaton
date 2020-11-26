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
	postProb = E_Step(xPoints, yPoints, B, t, sigmaSquared, w);
	
	for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
			REQUIRE(postProb(i,j) == doctest::Approx(postProbAnswer(i,j)));
        }
    }
}