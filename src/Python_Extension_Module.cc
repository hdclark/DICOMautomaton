//Python_Extension_Module.cc.

#include <pybind11/pybind11.h>

#include "Python_Bindings.h"

PYBIND11_MODULE(_dicomautomaton, module){
    dcma::python::BindModule(module);
}
