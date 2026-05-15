(define-module (dicomautomaton packages)
  #:use-module (gnu packages)
  #:use-module (guix build-system cmake)
  #:use-module (guix download)
  #:use-module (guix gexp)
  #:use-module (guix packages)
  #:use-module ((guix licenses) #:prefix license:))

(define %repo-root
  (canonicalize-path
   (or (getenv "DCMA_GUIX_REPO_ROOT")
       (getcwd))))

(define (github-archive owner repo ref version hash)
  (origin
    (method url-fetch)
    (uri (string-append "https://github.com/" owner "/" repo "/archive/" ref ".tar.gz"))
    (file-name (string-append repo "-" version ".tar.gz"))
    (sha256 (base32 hash))))

(define-public explicator
  (package
    (name "explicator")
    (version "2026.03.30-6e08569")
    (source
      (github-archive
        "hdclark"
        "Explicator"
        "6e085698aa36461733e1cee65070c50ff3ae411a"
        version
        "057aaqbnrwm4nhjghr5667fhswr2r598mjx8jr1x9r1cy2rgf2z3"))
    (build-system cmake-build-system)
    (arguments
      (list
        #:tests? #f
        #:build-type "Release"
        #:configure-flags #~(list "-DBUILD_SHARED_LIBS=ON")))
    (home-page "https://github.com/hdclark/Explicator")
    (synopsis "String explanation and translation library")
    (description
      "Explicator is a small C++ support library focused on string explanation and
translation tasks.")
    (license license:gpl3+)))

(define-public ygorclustering
  (package
    (name "ygorclustering")
    (version "2026.03.30-74d6f89")
    (source
      (github-archive
        "hdclark"
        "YgorClustering"
        "74d6f89c8f1a5112a55a4d7f349b7fc9a8971ad5"
        version
        "1gqdhraxdj0whkj2ax6ly78bpjn55wk4xycx32filvq85gcyz3q2"))
    (build-system cmake-build-system)
    (inputs
      (list (specification->package "boost")))
    (arguments
      (list
        #:tests? #f
        #:build-type "Release"
        #:configure-flags #~(list "-DBUILD_SHARED_LIBS=ON")))
    (home-page "https://github.com/hdclark/YgorClustering")
    (synopsis "Clustering support library used by DICOMautomaton")
    (description
      "YgorClustering is the clustering support library used by DICOMautomaton.")
    (license license:gpl3+)))

(define-public ygor
  (package
    (name "ygor")
    (version "2026.05.12-d641f1b")
    (source
      (github-archive
        "hdclark"
        "Ygor"
        "d641f1b2acb34d7520c99c49e9a2f57d99ffe5b6"
        version
        "0wb5n5q49d2lndiqc4z5hb0riaca3m679famjp93f0jqc9gg9gvi"))
    (build-system cmake-build-system)
    (native-inputs
      (list (specification->package "pkg-config")))
    (inputs
      (list
        (specification->package "boost")
        (specification->package "eigen")
        (specification->package "gsl")))
    (arguments
      (list
        #:tests? #f
        #:build-type "Release"
        #:configure-flags
        #~(list
            "-DWITH_EIGEN=ON"
            "-DWITH_BOOST=ON"
            "-DWITH_GNU_GSL=ON"
            "-DBUILD_SHARED_LIBS=ON")))
    (home-page "https://github.com/hdclark/Ygor")
    (synopsis "Scientific support library used by DICOMautomaton")
    (description
      "Ygor is the main scientific support library used by DICOMautomaton.")
    (license license:gpl3+)))

(define-public wt
  (package
    (name "wt")
    (version "4.13.2")
    (source
      (github-archive
        "emweb"
        "wt"
        "refs/tags/4.13.2"
        version
        "044mbgwzza8jzc032kq9d7ji2a6jb50fzzgfx42rsx15gz65lv0n"))
    (build-system cmake-build-system)
    (inputs
      (list
        (specification->package "boost")
        (specification->package "openssl")
        (specification->package "postgresql")
        (specification->package "zlib")))
    (arguments
      (list
        #:tests? #f
        #:build-type "Release"
        #:configure-flags
        #~(list
            "-DBUILD_TESTS=OFF"
            "-DINSTALL_EXAMPLES=OFF"
            "-DBUILD_SHARED_LIBS=ON")))
    (home-page "https://www.webtoolkit.eu/wt")
    (synopsis "C++ web toolkit")
    (description
      "Wt is a C++ web toolkit used by the DICOMautomaton web server build.")
    (license license:gpl2+)))

(define-public thrift
  (package
    (name "thrift")
    (version "0.23.0")
    (source
      (github-archive
        "apache"
        "thrift"
        "refs/tags/v0.23.0"
        version
        "023vm58p0qy84lp9wzxlwf4im7yqbqhaz07062g24xpgy5kr3msw"))
    (build-system cmake-build-system)
    (native-inputs
      (list (specification->package "pkg-config")))
    (inputs
      (list
        (specification->package "boost")
        (specification->package "openssl")
        (specification->package "zlib")))
    (arguments
      (list
        #:tests? #f
        #:build-type "Release"
        #:configure-flags
        #~(list
            "-DBUILD_TESTING=OFF"
            "-DBUILD_SHARED_LIBS=ON"
            "-DBUILD_COMPILER=OFF"
            "-DBUILD_CPP=ON"
            "-DBUILD_JAVA=OFF"
            "-DBUILD_NODEJS=OFF"
            "-DBUILD_PYTHON=OFF")))
    (home-page "https://thrift.apache.org/")
    (synopsis "Interface definition and RPC library")
    (description
      "Apache Thrift provides the RPC and serialization library needed for the
DICOMautomaton Thrift-enabled build.")
    (license license:asl2.0)))

(define %dicomautomaton-configure-flags
  #~(list
      "-DMEMORY_CONSTRAINED_BUILD=OFF"
      "-DWITH_ASAN=OFF"
      "-DWITH_TSAN=OFF"
      "-DWITH_MSAN=OFF"
      "-DWITH_FETCHCONTENT_FALLBACK=OFF"
      "-DWITH_EIGEN=ON"
      "-DWITH_CGAL=ON"
      "-DWITH_NLOPT=ON"
      "-DWITH_SFML=OFF"
      "-DWITH_SDL=ON"
      "-DWITH_WT=ON"
      "-DWITH_GNU_GSL=ON"
      "-DWITH_POSTGRES=ON"
      "-DWITH_JANSSON=ON"
      "-DWITH_THRIFT=ON"
      "-DWITH_EXT_SYCL=OFF"
      "-DBUILD_SHARED_LIBS=ON"))

(define-public dicomautomaton
  (package
    (name "dicomautomaton")
    (version "git-checkout")
    (source
      (local-file %repo-root "dicomautomaton-checkout" #:recursive? #t))
    (build-system cmake-build-system)
    (native-inputs
      (list (specification->package "pkg-config")))
    (inputs
      (list
        explicator
        thrift
        wt
        ygor
        ygorclustering
        (specification->package "asio")
        (specification->package "boost")
        (specification->package "cgal")
        (specification->package "eigen")
        (specification->package "glew")
        (specification->package "gmp")
        (specification->package "gsl")
        (specification->package "jansson")
        (specification->package "libpqxx")
        (specification->package "mpfr")
        (specification->package "nlopt")
        (specification->package "postgresql")
        (specification->package "sdl2")
        (specification->package "zlib")))
    (arguments
      (list
        #:tests? #f
        #:build-type "Release"
        #:configure-flags %dicomautomaton-configure-flags))
    (home-page "https://github.com/hdclark/DICOMautomaton")
    (synopsis "Medical physics automation toolkit")
    (description
      "DICOMautomaton is a collection of tools for analyzing radiotherapy,
medical imaging, geometry, and other medical physics data.")
    (license license:gpl3+)))

(define-public dicomautomaton-static
  (package
    (inherit dicomautomaton)
    (name "dicomautomaton-static")
    (arguments
      (substitute-keyword-arguments (package-arguments dicomautomaton)
        ((#:configure-flags flags)
         #~(append
             #$flags
             (list "-DBUILD_SHARED_LIBS=OFF")))
        ((#:phases phases #~%standard-phases)
         #~(modify-phases #$phases
             (add-before 'configure 'prefer-static-linking
               (lambda _
                 (setenv "PKG_CONFIG_ALL_STATIC" "1")))))))))
