
## `SYCL` Development

This directory contains a mock-up example of a blood perfusion model. In order to support GPU acceleration with `SYCL`,
the source code in this directory should be compiled with a `SYCL`-aware toolchain, specifically with the `syclcc`
compiler wrapper.

A `Docker` container is available that provides a `SYCL`-aware toolchain, along with many of the dependencies used by
`DICOMautomaton`. Re-using these dependencies will ensure the final code can be easily integrated into `DICOMautomaton`.

After installing `Docker`, the toolchain image can be downloaded using

    sudo docker pull hdclark/dcma_devel_arch_sycl:latest

You can run the image (on a `Linux` computer) via

    sudo docker run --rm -it --network=host hdclark/dcma_devel_arch_sycl:latest /bin/bash

and similarly the image can be run on `Windows` and `Apple` computers using `Docker for Windows` or `Docker for Macs`.
The running image is referred to as a container.

### Getting Started

The easiest workflow will be to:

1. Fork the `DICOMautomaton` repository <https://github.com.hdclark/DICOMautomaton> using the `GitHub` web interface.
2. Install `Docker`.
3. Launch the `hdclark/dcma_devel_arch_sycl` container in `Docker`.
4. `git clone` your forked `DICOMautomaton` repository.
5. Make changes to your implementation (in the `sycl/` directory) and commit them.
6. `git push` your commits to `GitHub`.
7. When your code seems to work, and you want to share with others, create a pull request with the main `DICOMautomaton`
   repository.

Note that development will likely continue, so you may have to rebase when you make a pull request. `GitHub` will let
you know if this is needed.

### Writing Code

The source files can be edited with any editor. The `Docker` image contains `vim`, but it is not necessary to use `vim`
if you prefer something else.

Note that when you exit the container, any changes you've made will be destroyed. If you edit your code inside the
container, save your changes either by regularly commiting and pushing to `GitHub` or by sharing a directory to save
files on the host computer (see the `docker run` `--volume` parameter).

The `Docker` image also contains `Boost`, `Eigen`, and `NLopt` libraries pre-configured, in case you want to use them.
They may not be necessary though!

### Compiling

`CMake` scripts are provided to simplify compiling, but the `SYCL` compiler is hard-coded. If new files are added, you
will have to add them to `src/CMakeLists.txt`. A helper script is provided to compile and install the code:

    ./compile_and_install.sh
    run_model -h

In order to test your code, you will need time course data (i.e., an AIF, a VIF, and at least one tissue time course). I
can provide these -- please let me know when you're ready.

A program that generates test files is also compiled. Running like so will create files `aif.txt`, `vif.txt`, and
`c.txt`. Feel free to modify it as necessary.

    gen_test_inputs

### Questions?

Please contact hal if you have questions, suggestions, or get stuck!

