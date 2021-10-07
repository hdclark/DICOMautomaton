
## Deformable Registration Development

This directory contains a mock-up example of a deformable registration algorithm. It can be used as a starting point to
implement a point cloud deformable registration algorithm of your choosing.

A `Docker` image is available that provides a development toolchain complete with everything needed to build
`DICOMautomaton`. Re-using the same dependencies as `DICOMautomaton` will simplify later integration of the registration
model.

After installing `Docker`, the toolchain image can be downloaded using

    sudo docker pull hdclark/dcma_build_base_arch:latest

You can run the image (on a `Linux` computer) via

    sudo docker run --rm -it --network=host hdclark/dcma_build_base_arch:latest /bin/bash

and similarly the image can be run on `Windows` and `Apple` computers using `Docker for Windows` or `Docker for Macs`.
The running image is referred to as a container.

### Getting Started

The easiest workflow will be to:

1. Fork the `DICOMautomaton` repository <https://github.com.hdclark/DICOMautomaton> using the `GitHub` web interface.
2. Install `Docker`.
3. Launch the `hdclark/dcma_build_base_arch` container in `Docker` on your computer.
4. `git clone` your forked `DICOMautomaton` repository.
5. Make changes to your implementation (in the `pc_registration/` directory) and commit them. (See below.)
6. `git push` your commits to your `GitHub` fork.
7. When your code seems to work, and you want to share with others, create a pull request with the main `DICOMautomaton`
   repository.

Note that `DICOMautomaton` development is relatively active, so you may have to rebase when you make a pull request.
`GitHub` will let you know if this is needed.

Absolutely feel free to figure out a workflow that works best for you!

### Writing Code

The source files can be edited with any editor. The `Docker` image contains `vim`, but it is not necessary to use `vim`
if you prefer something else.

Note that when you exit the container, any changes you've made will be destroyed. If you edit your code inside the
container, save your changes either by regularly commiting and pushing to `GitHub` or by sharing a directory to save
files on the host computer (see the `docker run` `--volume` parameter).

The `Docker` image also contains `Boost`, `Eigen`, and `NLopt` libraries pre-configured, in case you want to use them.
They may not be necessary though!

### Compiling

`CMake` scripts are provided to simplify compiling. To simplify later integration, please avoid using other dependencies
since they can limit portability. If new files are added, you
will have to add them to `src/CMakeLists.txt`.

You can compile your code using `pc_registration/compile_and_install.sh` and run the executable `run_def_reg`, which is
installed as a system-wide program.

    cd pc_registration/
    ./compile_and_install.sh
    run_model -h

In order to test your code, you will need point clouds in `XYZ` format. I can provide these -- please let me know when
you're ready.

### Web Development

A set of scripts were written to support using <https://repl.it> as a web-based development tool. Supplying the `GitHub`
address <https://github.com.hdclark/DICOMautomaton> will load a bare-bones development environment. The 'Run' button
will compile the sources in `pc_registration/`.

Please note that there are several caveats. The setup script takes around 10 minutes(!!) to complete, the installation
sometimes disconnects and needs to be re-generated, and the allocated resources are probably insufficient to load
anything but tiny point clouds. Using <https://repl.it> is, at best, only expected to be useful for discussion or
light development work.

### Questions?

Please contact hal if you have questions, suggestions, or get stuck!

