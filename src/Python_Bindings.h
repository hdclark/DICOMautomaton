//Python_Bindings.h.

#pragma once

#include <map>
#include <memory>
#include <string>

#include <pybind11/pybind11.h>

#include "Session.h"

namespace dcma {
namespace python {

struct EmbeddedSessionState {
    Drover *data = nullptr;
    std::map<std::string, std::string> *metadata = nullptr;
    const std::string *lexicon_filename = nullptr;
    bool active = true;
};

struct DataView {
    dcma::Session *session = nullptr;
    std::shared_ptr<EmbeddedSessionState> embedded;

    Drover & data() const;
};

struct EmbeddedSession {
    std::shared_ptr<EmbeddedSessionState> state;

    void require_active() const;
};

void BindModule(pybind11::module &module);

pybind11::object MakeEmbeddedSession(const std::shared_ptr<EmbeddedSessionState> &state);

} // namespace python
} // namespace dcma
