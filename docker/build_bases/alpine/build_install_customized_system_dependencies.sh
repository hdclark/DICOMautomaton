#!/usr/bin/env bash

set -eux

# Note: it might be possible to add this, but it wasn't specifically needed and didn't work on the first try...
#
## Install Wt from source to get around outdated and buggy Debian package.
##
## Note: Add additional dependencies if functionality is needed -- this is a basic install.
##
## Note: Could also install build-deps for the distribution packages, but the dependencies are not
##       guaranteed to be stable (e.g., major version bumps).
#(
#    cd / && rm -rf /wt*
#    git clone https://github.com/emweb/wt.git /wt
#    cd /wt
#    mkdir -p build && cd build
#    CXXFLAGS="-static -fPIC" \
#    cmake \
#      -DCMAKE_INSTALL_PREFIX=/usr \
#      -DBUILD_SHARED=OFF \
#      -DBoost_USE_STATIC_LIBS=ON \
#      ../
#    make -j "$JOBS" VERBOSE=0
#    make install
#    make clean
#)

# Remove these to trim custom boost build deps.
apk del --no-cache bzip2 xz icu 

# boost.
(
    cd / && rm -rf /boost* || true
    wget 'https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.gz' 
    tar -axf 'boost_1_77_0.tar.gz' 
    rm 'boost_1_77_0.tar.gz' 
    cd /boost_1_77_0/tools/build/src/engine/
    printf '%s\n' "using gcc : : ${CXX} ${CXXFLAGS} : <link>static <runtime-link>static <cflags>${CFLAGS} <cxxflags>${CXXFLAGS} <linkflags>${LDFLAGS} ;" > /user-config.jam
    printf '%s\n' "using zlib : : <search>/usr/lib <name>zlib <include>/use/include ;" >> /user-config.jam
    ./build.sh --cxx="g++"
    cd ../../
    ./bootstrap.sh --cxx="g++"
    ./b2 --prefix=/ install
    cd ../../
    ./tools/build/src/engine/b2 -j"$JOBS" \
        --user-config=/user-config.jam \
        --prefix=/ \
        release \
        toolset=gcc \
        debug-symbols=off \
        threading=multi \
        runtime-link=static \
        link=static \
        cflags='-fno-use-cxa-atexit -fno-strict-aliasing' \
        --includedir=/usr/include \
        --libdir=/usr/lib \
        --with-iostreams \
        --with-serialization \
        --with-thread \
        --with-system \
        --with-filesystem \
        --with-regex \
        --with-chrono \
        --with-date_time \
        --with-atomic \
        -sZLIB_NAME=libz \
        -sZLIB_INCLUDE=/usr/include/ \
        -sZLIB_LIBRARY_PATH=/usr/lib/ \
        install
    ls -lash /usr/lib
    cd / && rm -rf /boost* || true
)

apk add --no-cache bzip2 xz unzip

# Asio.
(
    cd / && rm -rf /asio || true
    mkdir -pv /asio
    cd /asio
    wget 'https://sourceforge.net/projects/asio/files/latest/download' -O asio.tgz
    tar -axf asio.tgz || unzip asio.tgz
    cd asio-*/
    cp -v -R include/asio/ include/asio.hpp /usr/include/
    cd / && rm -rf /asio || true
)

# Tinkering with minimal static mesa build. Currently doesn't seem like there is a way forward.
#
#apk add --no-cache libx11-dev libx11-static glu-dev glu mesa-dev mesa-dev
#apk add --no-cache llvm11-static llvm11-dev bison flex libxrandr-dev libdrm-dev
#
## mesa.
#(
#    cd / && rm -rf /mesa || true
#    git clone --depth 1 "https://github.com/mesa3d/mesa.git" /mesa
#    # meson configure /mesa | less
#    meson /mesa /mesa_build \
#        -D buildtype=release \
#        -D debug=false \
#        -D default_library=static \
#        -D strip=false \
#        \
#        -D b_lto=false \
#        -D b_ndebug=false \
#        -D b_staticpic=true \
#        \
#        -D platforms=wayland \
#        \
#        -D dri3=disabled \
#        -D draw-use-llvm=true \
#        -D dri-drivers= \
#        -D egl=disabled \
#        -D gallium-drivers=zink \
#        -D gles1=disabled \
#        -D gles2=disabled \
#        -D glvnd=false \
#        -D glx=disabled \
#        -D libunwind=disabled \
#        -D llvm=enabled \
#        -D lmsensors=disabled \
#        -D microsoft-clc=disabled \
#        -D opencl-native=false \
#        -D opencl-spirv=false \
#        -D opengl=true \
#        -D osmesa=true \
#        -D selinux=false \
#        -D shader-cache=false \
#        -D shared-glapi=disabled \
#        -D shared-llvm=false \
#        -D shared-swr=false \
#        -D swr-arches=avx \
#        -D valgrind=false \
#        -D vulkan-drivers= \
#        -D zlib=enabled \
#        -D zstd=disabled
#    ninja -C /mesa_build
#    meson compile -C /meson_build
#    meson install -C /meson_build
#
#    cd / && rm -rf /asio || true
#)

## Attempt to use PortableGL in lieu of mesa.
#(
#    #GLES3/gl3.h
#    #GLES2/gl2.h
#    #GLES2/gl2ext.h
#    wget 'https://raw.githubusercontent.com/rswinkle/PortableGL/master/portablegl.h' -O /usr/include/portablegl.h
#    ##sed -i -e 's/[#]include <GLES3\/gl3[.]h>/#define MANGLE_TYPES\n#define PORTABLEGL_IMPLEMENTATION\n#include <portablegl.h>/g' src/imgui20210904/imgui_impl_opengl3.cpp
#    sed -i -e 's/[#]include <GLES3\/gl3[.]h>/#define MANGLE_TYPES\n#include <portablegl.h>/g' src/imgui20210904/imgui_impl_opengl3.cpp
#
#    # Remove inclusion of glew.h and use of glew... functions from SDL_Viewer.cc
#    # ...
#
#    # I also purged all CHECK_FOR_GL_ERRORS, but maybe better to redefine if possible.
#    # ...
#
#    # Replace GL_UNPACK_ROW_LENGTH as used in 'glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);' with something.
#    # ... (I just deleted it)
#
#    # Add to SDL_Viewer.cc , preferrably at the bottom of the inclues list.
#      #define MANGLE_TYPES
#      #define PORTABLEGL_IMPLEMENTATION
#      #include <portablegl.h>
#
#rm /usr/lib/libGL.so
#rm /usr/lib/libGLU.so
#touch /usr/lib/libGL.a
#touch /usr/lib/libGLU.a
## Not sure why CMake insists on .so files here?
## CMake emits '-- Proceeding with OPENGL_LIBRARIES = /usr/lib/libGL.so;/usr/lib/libGLU.so'
## The following seems to fix the immediate linking issue though.
#touch /usr/lib/libGL.so
#touch /usr/lib/libGLU.so
#
#    # Here is the current diff that compiles:
#
#    #    diff --git a/docker/build_bases/alpine/build_install_dcma.sh b/docker/build_bases/alpine/build_install_dcma.sh
#    #    index 19fc5435..17d4df31 100755
#    #    --- a/docker/build_bases/alpine/build_install_dcma.sh
#    #    +++ b/docker/build_bases/alpine/build_install_dcma.sh
#    #    @@ -2,6 +2,8 @@
#    #     
#    #     set -eux
#    #     
#    #    +apk add libx11-dev libx11-static glu-dev glu mesa-dev mesa-dev
#    #    +
#    #     # DICOMautomaton.
#    #     (
#    #         if [ ! -d /dcma ] ; then
#    #    diff --git a/src/CMakeLists.txt b/src/CMakeLists.txt
#    #    index 47b3f4dd..3a0b67a7 100644
#    #    --- a/src/CMakeLists.txt
#    #    +++ b/src/CMakeLists.txt
#    #    @@ -365,8 +365,8 @@ target_link_libraries (dicomautomaton_dispatcher
#    #         Boost::system
#    #         z
#    #         "$<$<BOOL:${BUILD_SHARED_LIBS}>:${CMAKE_DL_LIBS}>"
#    #    -    mpfr
#    #    -    gmp
#    #    +
#    #    +
#    #         m
#    #         Threads::Threads
#    #     )
#    #    @@ -446,8 +446,8 @@ if(WITH_WT)
#    #             Boost::system
#    #             z
#    #             "$<$<BOOL:${BUILD_SHARED_LIBS}>:${CMAKE_DL_LIBS}>"
#    #    -        mpfr
#    #    -        gmp
#    #    +
#    #    +
#    #             m
#    #             Threads::Threads
#    #         )
#    #    diff --git a/src/Operations/SDL_Viewer.cc b/src/Operations/SDL_Viewer.cc
#    #    index fbf7e2d1..6c5fc51c 100644
#    #    --- a/src/Operations/SDL_Viewer.cc
#    #    +++ b/src/Operations/SDL_Viewer.cc
#    #    @@ -44,7 +44,6 @@
#    #     #include "../implot20210904/implot.h"
#    #     
#    #     #include <SDL.h>
#    #    -#include <GL/glew.h>            // Initialize with glewInit()
#    #     
#    #     #include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#    #     #include "YgorImages.h"
#    #    @@ -81,6 +80,10 @@
#    #     
#    #     #include "SDL_Viewer.h"
#    #     
#    #    +#define MANGLE_TYPES
#    #    +#define PORTABLEGL_IMPLEMENTATION
#    #    +#include <portablegl.h>
#    #    +
#    #     //extern const std::string DCMA_VERSION_STR;
#    #     
#    #     static
#    #    @@ -564,20 +567,6 @@ bool SDL_Viewer(Drover &DICOM_data,
#    #         string_to_array(metadata_text_entry, "");
#    #    
#    #         // --------------------------------------------- Setup ------------------------------------------------
#    #    -#ifndef CHECK_FOR_GL_ERRORS
#    #    -    #define CHECK_FOR_GL_ERRORS() { \
#    #    -        while(true){ \
#    #    -            GLenum err = glGetError(); \
#    #    -            if(err == GL_NO_ERROR) break; \
#    #    -            std::cout << "--(W) In function: " << __PRETTY_FUNCTION__; \
#    #    -            std::cout << " (line " << __LINE__ << ")"; \
#    #    -            std::cout << " : " << glewGetErrorString(err); \
#    #    -            std::cout << "(" << std::to_string(err) << ")." << std::endl; \
#    #    -            std::cout.flush(); \
#    #    -            throw std::runtime_error("OpenGL error detected. Refusing to continue"); \
#    #    -        } \
#    #    -    }
#    #    -#endif
#    #    
#    #         if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0){
#    #             throw std::runtime_error("Unable to initialize SDL: "_s + SDL_GetError());
#    #    @@ -628,15 +617,6 @@ bool SDL_Viewer(Drover &DICOM_data,
#    #             }
#    #         }
#    #    
#    #    -    glewExperimental = true; // Bug fix for glew v1.13.0 and earlier.
#    #    -    if(glewInit() != GLEW_OK){
#    #    -        throw std::runtime_error("Glew was unable to initialize OpenGL");
#    #    -    }
#    #    -    try{
#    #    -        CHECK_FOR_GL_ERRORS(); // Clear any errors encountered during glewInit.
#    #    -    }catch(const std::exception &e){
#    #    -        FUNCINFO("Ignoring glew-related error: " << e.what());
#    #    -    }
#    #    
#    #         // Create an ImGui context we can use and associate it with the OpenGL context.
#    #         IMGUI_CHECKVERSION();
#    #    @@ -649,15 +629,12 @@ bool SDL_Viewer(Drover &DICOM_data,
#    #         ImGui::StyleColorsDark();
#    #    
#    #         // Setup Platform/Renderer backends
#    #    -    CHECK_FOR_GL_ERRORS();
#    #         if(!ImGui_ImplSDL2_InitForOpenGL(window, gl_context)){
#    #             throw std::runtime_error("ImGui unable to associate SDL window with OpenGL context.");
#    #         }
#    #    -    CHECK_FOR_GL_ERRORS();
#    #         if(!ImGui_ImplOpenGL3_Init()){
#    #             throw std::runtime_error("ImGui unable to initialize OpenGL with given shader.");
#    #         }
#    #    -    CHECK_FOR_GL_ERRORS();
#    #         try{
#    #             auto gl_version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
#    #             auto glsl_version = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));
#    #    @@ -957,26 +934,22 @@ bool SDL_Viewer(Drover &DICOM_data,
#    #                 out.aspect_ratio = (img.pxl_dx / img.pxl_dy) * (static_cast<float>(img_rows) / static_cast<float>(img_cols));
#    #                 out.aspect_ratio = std::isfinite(out.aspect_ratio) ? out.aspect_ratio : (img.pxl_dx / img.pxl_dy);
#    #    
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #                 glGenTextures(1, &out.texture_number);
#    #                 glBindTexture(GL_TEXTURE_2D, out.texture_number);
#    #                 if(out.texture_number == 0){
#    #                     throw std::runtime_error("Unable to assign OpenGL texture");
#    #                 }
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #    
#    #                 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#    #                 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #    -            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#    #    +            //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#    #                 glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#    #                 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
#    #                              static_cast<int>(out.col_count), static_cast<int>(out.row_count),
#    #                              0, GL_RGB, GL_UNSIGNED_BYTE, static_cast<void*>(animage.data()));
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #                 out.texture_exists = true;
#    #                 return out;
#    #    @@ -4214,14 +4187,12 @@ script_files.back().content.emplace_back('\0');
#    #             }
#    #    
#    #             // Clear the current OpenGL frame.
#    #    -        CHECK_FOR_GL_ERRORS();
#    #             glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
#    #             glClearColor(background_colour.x,
#    #                          background_colour.y,
#    #                          background_colour.z,
#    #                          background_colour.w);
#    #             glClear(GL_COLOR_BUFFER_BIT);
#    #    -        CHECK_FOR_GL_ERRORS();
#    #    
#    #    
#    #             // Tinkering with rendering surface meshes.
#    #    @@ -4307,7 +4278,6 @@ script_files.back().content.emplace_back('\0');
#    #                 GLuint vbo = 0;
#    #                 GLuint ebo = 0;
#    #    
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #                 glGenVertexArrays(1, &vao); // Create a VAO inside the OpenGL context.
#    #                 glGenBuffers(1, &vbo); // Create a VBO inside the OpenGL context.
#    #    @@ -4316,41 +4286,33 @@ script_files.back().content.emplace_back('\0');
#    #                 glBindVertexArray(vao); // Bind = make it the currently-used object.
#    #                 glBindBuffer(GL_ARRAY_BUFFER, vbo);
#    #    
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #                 glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), static_cast<void*>(vertices.data()), GL_STATIC_DRAW); // Copy vertex data.
#    #    
#    #                 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
#    #                 glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), static_cast<void*>(indices.data()), GL_STATIC_DRAW); // Copy index data.
#    #    
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #                 glEnableVertexAttribArray(0); // enable attribute with index 0.
#    #                 glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); // Vertex positions, 3 floats per vertex, attrib index 0.
#    #    
#    #                 glBindVertexArray(0);
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #    
#    #                 // Draw the mesh.
#    #    -            CHECK_FOR_GL_ERRORS();
#    #                 glBindVertexArray(vao); // Bind = make it the currently-used object.
#    #    
#    #                 glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Enable wireframe mode.
#    #    -            CHECK_FOR_GL_ERRORS();
#    #                 glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0); // Draw using the current shader setup.
#    #    -            CHECK_FOR_GL_ERRORS();
#    #                 glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Disable wireframe mode.
#    #    
#    #                 glBindVertexArray(0);
#    #    -            CHECK_FOR_GL_ERRORS();
#    #    
#    #                 glDisableVertexAttribArray(0); // Free OpenGL resources.
#    #                 glDisableVertexAttribArray(1);
#    #                 glDeleteBuffers(1, &ebo);
#    #                 glDeleteBuffers(1, &vbo);
#    #                 glDeleteVertexArrays(1, &vao);
#    #    -            CHECK_FOR_GL_ERRORS();
#    #                 return;
#    #             };
#    #             draw_surface_meshes();
#    #    diff --git a/src/imgui20210904/imgui_impl_opengl3.cpp b/src/imgui20210904/imgui_impl_opengl3.cpp
#    #    index 5637a0fe..9c33a3da 100644
#    #    --- a/src/imgui20210904/imgui_impl_opengl3.cpp
#    #    +++ b/src/imgui20210904/imgui_impl_opengl3.cpp
#    #    @@ -108,7 +108,9 @@
#    #     #if (defined(__APPLE__) && (TARGET_OS_IOS || TARGET_OS_TV))
#    #     #include <OpenGLES/ES3/gl.h>    // Use GL ES 3
#    #     #else
#    #    -#include <GLES3/gl3.h>          // Use GL ES 3
#    #    +#define MANGLE_TYPES
#    #    +#define PORTABLEGL_IMPLEMENTATION
#    #    +#include <portablegl.h>          // Use GL ES 3
#    #     #endif
#    #     #elif !defined(IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
#    #     // Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
#    #    
#    #    ~
#
#
#
#    # Now when I run the binary on another machine, it fails like so:
#    #    [hal@trip tmp]$ ./dicomautomaton_dispatcher -v -o GenerateVirtualDataImageSphere -o SDL_Viewer
#    #    --(W) In function: Operation_Dispatcher: Selecting operation 'GenerateVirtualDataImageSphereV1' because 'GenerateVirtualDataImageSphere' not understood.
#    #    --(I) In function: Operation_Dispatcher: Performing operation 'GenerateVirtualDataImageSphereV1' now...
#    #    --(I) In function: Operation_Dispatcher: Performing operation 'SDL_Viewer' now...
#    #    --(I) In function: operator(): Worker thread ready.
#    #    --(W) In function: Operation_Dispatcher: Analysis failed: 'Unable to initialize SDL: No available video device'. Aborting remaining analyses.
#    #    --(E) In function: main: Analysis failed. Cannot continue. Terminating program.
#
#    # Using strace, it is indeed looking at /dev/dri/card0 (which exists):
#    #    ...
#    #    munmap(0x7fbe53a1c000, 4096)            = 0
#    #    mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fbe53a1c000
#    #    rt_sigaction(SIGINT, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) = 0
#    #    rt_sigaction(SIGINT, {sa_handler=0x1233733, sa_mask=[], sa_flags=SA_RESTORER, sa_restorer=0x1380512}, NULL, 8) = 0
#    #    rt_sigaction(SIGTERM, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) = 0
#    #    rt_sigaction(SIGTERM, {sa_handler=0x1233733, sa_mask=[], sa_flags=SA_RESTORER, sa_restorer=0x1380512}, NULL, 8) = 0
#    #    mmap(NULL, 143360, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fbe53474000
#    #    mprotect(0x7fbe53476000, 135168, PROT_READ|PROT_WRITE) = 0
#    #    rt_sigprocmask(SIG_BLOCK, ~[RTMIN RT_1 RT_2], [], 8) = 0
#    #    clone(child_stack=0x7fbe53496998, flags=CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID|0x400000, parent_tid=[2198117], tls=0x7fbe53496b30, child_tidptr=0x1fe5e90) = 2198117
#    #    rt_sigprocmask(SIG_SETMASK, [], NULL, 8) = 0
#    #    stat("/dev/dri/", {st_mode=S_IFDIR|0755, st_size=100, ...}) = 0
#    #    access("/dev/dri/", F_OK)               = 0
#    #    open("/dev/dri/", O_RDONLY|O_LARGEFILE|O_CLOEXEC|O_DIRECTORY) = 3
#    #    fcntl(3, F_SETFD, FD_CLOEXEC)           = 0
#    #    mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fbe53a0a000
#    #    getdents64(3, 0x7fbe53a0a218 /* 5 entries */, 2048) = 144
#    #    getdents64(3, 0x7fbe53a0a218 /* 0 entries */, 2048) = 0
#    #    close(3)                                = 0
#    #    munmap(0x7fbe53a0a000, 8192)            = 0
#    #    open("/dev/dri/card0", O_RDWR|O_LARGEFILE|O_CLOEXEC) = 3
#    #    fcntl(3, F_SETFD, FD_CLOEXEC)           = 0
#    #    close(3)                                = 0
#    #    stat("/dev/dri/", {st_mode=S_IFDIR|0755, st_size=100, ...}) = 0
#    #    access("/dev/dri/", F_OK)               = 0
#    #    open("/dev/dri/", O_RDONLY|O_LARGEFILE|O_CLOEXEC|O_DIRECTORY) = 3
#    #    fcntl(3, F_SETFD, FD_CLOEXEC)           = 0
#    #    mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fbe53a0a000
#    #    getdents64(3, 0x7fbe53a0a228 /* 5 entries */, 2048) = 144
#    #    getdents64(3, 0x7fbe53a0a228 /* 0 entries */, 2048) = 0
#    #    close(3)                                = 0
#    #    munmap(0x7fbe53a0a000, 8192)            = 0
#    #    ...
#    #    mmap(NULL, 253004, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fbe53436000
#    #    mmap(NULL, 253004, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fbe533f8000
#    #    munmap(0x7fbe533f8000, 253952)          = 0
#    #    munmap(0x7fbe53a1c000, 4096)            = 0
#    #    munmap(0x7fbe53a0c000, 65536)           = 0
#    #    futex(0x7fbe534b9874, FUTEX_WAKE_PRIVATE, 1) = 1
#    #    futex(0x7fbe534b9b68, FUTEX_WAIT_PRIVATE, 2, NULL) = -1 EAGAIN (Resource temporarily unavailable)
#    #    munmap(0x7fbe53497000, 143360)          = 0
#    #    writev(1, [{iov_base="--(W) In function: Operation_Dis"..., iov_len=141}, {iov_base="\n", iov_len=1}], 2--(W) In function: Operation_Dispatcher: Analysis failed: 'Unable to initialize SDL: No available video device'. Aborting remaining analyses.
#    #    ) = 142
#    #    munmap(0x7fbe53a25000, 16384)           = 0
#    #    munmap(0x7fbe53a2d000, 8192)            = 0
#    #    munmap(0x7fbe53a35000, 8192)            = 0
#    #    munmap(0x7fbe53a29000, 8192)            = 0
#    #    munmap(0x7fbe53a39000, 4096)            = 0
#    #    munmap(0x7fbe53a3a000, 8192)            = 0
#    #    munmap(0x7fbe53a3c000, 4096)            = 0
#    #    munmap(0x7fbe53a3d000, 4096)            = 0
#    #    munmap(0x7fbe53a3e000, 8192)            = 0
#    #    writev(2, [{iov_base="", iov_len=0}, {iov_base="--(E) In function: ", iov_len=19}], 2--(E) In function: ) = 19
#    #    writev(2, [{iov_base="", iov_len=0}, {iov_base="main", iov_len=4}], 2main) = 4
#    #    writev(2, [{iov_base="", iov_len=0}, {iov_base=": ", iov_len=2}], 2: ) = 2
#    #    writev(2, [{iov_base="", iov_len=0}, {iov_base="Analysis failed. Cannot continue", iov_len=32}], 2Analysis failed. Cannot continue) = 32
#    #    writev(2, [{iov_base="", iov_len=0}, {iov_base=". Terminating program.", iov_len=22}], 2. Terminating program.) = 22
#    #    writev(2, [{iov_base="", iov_len=0}, {iov_base="\n", iov_len=1}], 2
#    #    ) = 1
#    #    munmap(0x7fbe53a52000, 4096)            = 0
#    #    munmap(0x7fbe53a54000, 4096)            = 0
#    #    munmap(0x7fbe53436000, 253952)          = 0
#    #    exit_group(-1)                          = ?
#    #    +++ exited with 255 +++
#    #
#    #
#    # Hopefully I'm just missing the glew loading code. It's too bad portableGL and glew don't interoperate.
#
#)

