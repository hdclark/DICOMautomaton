# Point Set Registration: Coherent Point Drift

Implementation of the Rigid, Affine, and Nonrigid Coherent Point Drift algorithms for point set registration.

## Setup
```
$ cd registration
$ ./compile_and_install.sh
```

## Usage
To run the coherent point drift algorithm:
```
$ run_def_reg [arguments]
arguments:
  -m File to load moving point cloud from.
  -s File to load stationary point cloud from.
  -t Coherent point drift algorithm to use. Options: rigid, affine, nonrigid
  -p Write transformed point set to given file.
  -o Write transform matrix to given file.
  -d Number of iterations between saving point set.
  -v Boolean to represent whether to plot for a video.
  -w Weight of the uniform distribution. 0 <= w <= 1 (Optional, default w=0.2)
  -l Trade-off between the goodness of maximum likelihood fit and regularization. l > 0 (Optional, default l=2)
  -b Defines the model of the smoothness regularizer - width of smoothing Gaussian filter. b>0 (Optional, default b=2)
  -i Maximum number of iterations for algorithm. (Optional, default i=100)
  -r Similarity threshold to terminate iteratiosn at. (Optional, default r=1)
  -f Use fast gauss tranform for Nonrigid CPD, will have no effect for other algorithms.(Optional, default False)
```
## Unit Tests
To run unit tests:
```
$ cd registration/unit_tests
$ ./compile_and_run.sh
```
