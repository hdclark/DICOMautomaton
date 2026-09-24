//Python_Embed.cc.

#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Python.h>
#include <pybind11/pybind11.h>

#include "Python_Bindings.h"
#include "Python_Embed.h"
#include "Process_Fork.h"

namespace py = pybind11;

extern "C" PyObject *PyInit__dicomautomaton_embedded();

namespace {

class InterpreterManager {
    public:
        void ensure_initialized(){
            std::call_once(initialization_, [this](){
                if(Py_IsInitialized()){
                    DisableForkForPythonInitialization();
                    owns_interpreter_ = false;
                    return;
                }

                // Block new POSIX forks before CPython begins creating process-global state.
                DisableForkForPythonInitialization();

                if(PyImport_AppendInittab("_dicomautomaton_embedded",
                                         &PyInit__dicomautomaton_embedded) != 0){
                    throw std::runtime_error("unable to register the embedded DICOMautomaton Python module");
                }

                PyConfig config;
                PyConfig_InitIsolatedConfig(&config);
                config.install_signal_handlers = 0;
                const auto status = Py_InitializeFromConfig(&config);
                PyConfig_Clear(&config);
                if(PyStatus_Exception(status)){
                    throw std::runtime_error(std::string("unable to initialize CPython: ")
                                             + (status.err_msg ? status.err_msg : "unknown error"));
                }

                owns_interpreter_ = true;
                auto *module = PyImport_ImportModule("_dicomautomaton_embedded");
                if(!module){
                    {
                        const py::error_already_set error;
                        initialization_error_ = "unable to initialize the embedded DICOMautomaton Python module: ";
                        initialization_error_ += error.what();
                    }
                    PyEval_SaveThread();
                    return;
                }
                if(PyDict_SetItemString(PyImport_GetModuleDict(),
                                         "dicomautomaton._dicomautomaton", module) != 0){
                    Py_DECREF(module);
                    {
                        const py::error_already_set error;
                        initialization_error_ = "unable to alias the embedded DICOMautomaton Python module: ";
                        initialization_error_ += error.what();
                    }
                    PyEval_SaveThread();
                    return;
                }
                Py_DECREF(module);
                PyEval_SaveThread();
            });
            if(!initialization_error_.empty()){
                throw std::runtime_error(initialization_error_);
            }
        }

        bool owns_interpreter() const {
            return owns_interpreter_;
        }

    private:
        std::once_flag initialization_;
        bool owns_interpreter_ = false;
        std::string initialization_error_;
};

InterpreterManager & interpreter_manager(){
    // CPython is deliberately not finalized during ordinary process shutdown.
    static auto *manager = new InterpreterManager();
    return *manager;
}

std::recursive_mutex & execution_mutex(){
    static auto *mutex = new std::recursive_mutex();
    return *mutex;
}

std::string read_file(const std::string &filename){
    std::ifstream is(filename, std::ios::binary);
    if(!is){
        throw std::invalid_argument("unable to open Python script file '" + filename + "'");
    }
    std::ostringstream os;
    os << is.rdbuf();
    if(is.bad()){
        throw std::runtime_error("unable to read Python script file '" + filename + "'");
    }
    return os.str();
}

} // namespace

PYBIND11_MODULE(_dicomautomaton_embedded, module){
    dcma::python::BindModule(module);
}

namespace dcma {
namespace python {

void ExecuteScriptFile(Drover &data,
                       std::map<std::string, std::string> &metadata,
                       const std::string &lexicon_filename,
                       const std::string &filename,
                       const std::string &function,
                       const std::vector<std::string> &module_search_paths){
    const auto source = read_file(filename);
    auto &manager = interpreter_manager();
    manager.ensure_initialized();

    // sys.path is process-global. Lock before acquiring the GIL so another
    // callback cannot hold the GIL while waiting for an active callback.
    std::lock_guard<std::recursive_mutex> execution_lock(execution_mutex());
    py::gil_scoped_acquire acquire;
    if(!manager.owns_interpreter()){
        auto *modules = PyImport_GetModuleDict();
        auto *module = PyDict_GetItemString(modules, "dicomautomaton._dicomautomaton");
        Py_XINCREF(module);
        if(!module){
            module = PyInit__dicomautomaton_embedded();
            if(!module){
                throw py::error_already_set();
            }
            if((PyDict_SetItemString(modules, "_dicomautomaton_embedded", module) != 0)
            || (PyDict_SetItemString(modules, "dicomautomaton._dicomautomaton", module) != 0)){
                Py_DECREF(module);
                throw py::error_already_set();
            }
        }
        Py_DECREF(module);
    }

    auto state = std::make_shared<EmbeddedSessionState>();
    state->data = &data;
    state->metadata = &metadata;
    state->lexicon_filename = &lexicon_filename;
    const auto invalidate = [&state](){
        state->active = false;
        state->data = nullptr;
        state->metadata = nullptr;
        state->lexicon_filename = nullptr;
    };

    auto sys = py::module::import("sys");
    const py::object original_path = sys.attr("path");
    try{
        auto path = py::list(original_path);
        for(auto iter = module_search_paths.rbegin(); iter != module_search_paths.rend(); ++iter){
            path.attr("insert")(0, *iter);
        }
        sys.attr("path") = path;

        py::dict globals;
        globals["__builtins__"] = py::module::import("builtins");
        globals["__file__"] = filename;
        globals["__name__"] = "__dicomautomaton_operation__";
        auto builtins = py::module::import("builtins");
        auto code = builtins.attr("compile")(source, filename, "exec");
        builtins.attr("exec")(code, globals, globals);

        auto *callback = PyDict_GetItemString(globals.ptr(), function.c_str());
        if(!callback || !PyCallable_Check(callback)){
            throw std::invalid_argument("Python script '" + filename
                                        + "' does not define callable '" + function + "'");
        }
        py::reinterpret_borrow<py::object>(callback)(MakeEmbeddedSession(state));
        invalidate();
        sys.attr("path") = original_path;
    }catch(const py::error_already_set &e){
        invalidate();
        sys.attr("path") = original_path;
        throw std::runtime_error("Python callback '" + function + "' in '" + filename
                                 + "' failed:\n" + e.what());
    }catch(...){
        invalidate();
        sys.attr("path") = original_path;
        throw;
    }
}

} // namespace python
} // namespace dcma
