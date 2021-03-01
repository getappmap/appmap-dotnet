#include <doctest/doctest.h>
#include <fakeit.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "method.h"

using namespace fakeit;
using namespace std::filesystem;

REGISTER_EXCEPTION_TRANSLATOR(fakeit::FakeitException &ex) {
    return ex.what().c_str();
}

template <typename T>
struct ComMock : public Mock<T>
{
    ComMock() {
        Fake(Method((*this), AddRef), Method((*this), Release));
        When(Method((*this), QueryInterface)).AlwaysDo([this](REFIID, void **ppvObject) {
            *ppvObject = &this->get();
            return S_OK;
        });
    }
};

struct temp_directory : public path
{
    temp_directory() : path(std::tmpnam(nullptr)) {
        create_directories(*this);
    }

    ~temp_directory() {
        remove_all(*this);
    }
};

TEST_CASE("application name set in config file is applied") {
    const temp_directory basedir;
    current_path(basedir);
    std::ofstream(basedir / "appmap.yml") << "name: appmap-test";

    ComMock<IProfilerManager> manager;
    ComMock<ICorProfilerInfo> info;

    When(Method(manager, GetCorProfilerInfo)).Do([&info](auto result) {
        *result = &info.get();
        return S_OK;
    });
    Fake(Method(info, GetEventMask), Method(info, SetEventMask));

    appmap::instrumentation_method method;
    method.initialize(&manager.get());
    CHECK(method.appmap()["metadata"]["app"] == "appmap-test");
}
