#include <algorithm>
#include <doctest/doctest.h>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include <utf8.h>
#include <spdlog/fmt/bundled/ostream.h>
#include <fstream>

#include "config.h"

namespace fs = std::filesystem;
using namespace appmap;

namespace {
    std::optional<std::string> get_envar(const char *name) {
        const auto result = std::getenv(name);
        if (result) {
            return result;
        } else {
            return std::nullopt;
        }
    }

    bool get_bool_envar(const char *name) {
        const auto result = std::getenv(name);
        if (result == nullptr)
            return false;
        else switch(tolower(result[0])) {
            case '1':
            case 'y':
            case 't':
            case 'j':
                return true;
            case 'o':
                return tolower(result[1]) == 'n';
            default:
                return false;
        }
    }

    std::optional<fs::path> find_file(const std::string name, const fs::path &basepath) {
        for (fs::path dir = basepath; dir != dir.root_path(); dir = dir.parent_path()) {
            const auto file = dir / name;
            if (fs::exists(file))
                return file;
        }

        spdlog::warn("no {} found in {}", name, basepath.string());

        return std::nullopt;
    }

    std::optional<fs::path> config_file_path(const fs::path &basepath) {
        const auto from_env = get_envar("APPMAP_CONFIG");
        if (from_env)
            return from_env;
        else
            return find_file("appmap.yml", basepath);
    }

    void load_config(appmap::config &c, const YAML::Node &config_file) {
        if (const auto &pkgs = config_file["packages"]) {
            for (const auto &pkg: pkgs) {
                if (pkg.IsScalar()) {
                    c.classes.push_back(pkg.as<std::string>());
                    continue;
                } else if (pkg.IsMap()) {
                    if (const auto &mod = pkg["module"]) {
                        c.modules.push_back(mod.as<std::string>());
                        continue;
                    } else if (const auto &cls = pkg["class"]) {
                        c.classes.push_back(cls.as<std::string>());
                        continue;
                    } else if (const auto &path = pkg["path"]) {
                        c.paths.push_back(path.as<std::string>());
                        continue;
                    }
                }
                spdlog::warn("unrecognized package specification in config file: {}", pkg);
            }
        }
    }

    appmap::config load_default()
    {
        config c;
        c.module_list_path = get_envar("APPMAP_LIST_MODULES");
        c.appmap_output_path = get_envar("APPMAP_OUTPUT_PATH");
        c.generate_classmap = get_bool_envar("APPMAP_CLASSMAP");
        const auto basepath = get_envar("APPMAP_BASEPATH");
        if (basepath)
            c.base_path = *basepath;

        // it's probably not the best place for this, but it'll do
        if (const auto &log_level = get_envar("APPMAP_LOG_LEVEL"))
            spdlog::set_level(spdlog::level::from_str(*log_level));
        else
            spdlog::set_level(spdlog::level::info);

        if (const auto config_path = config_file_path(basepath.value_or(fs::current_path()))) {
            if (!basepath)
                c.base_path = (*config_path).parent_path();
            load_config(c, YAML::LoadFile(*config_path));
        } else {
            spdlog::critical("appmap configuration file not found");
        }

        return c;
    }

    fs::path resolve(const fs::path &path, const fs::path &base) {
        assert(path.is_relative());
        fs::path res = fs::weakly_canonical(base / path);
        if (res.filename().empty())
            res = res.parent_path();
        return res;
    }

    std::string sanitize_filename(const std::string &name)
    {
        std::string result = name;
        for (char &c : result) {
            if (c == '/' || c == fs::path::preferred_separator)
                c = '-';
        }
        return result;
    }

    std::unique_ptr<std::ostream> output_file(const std::filesystem::path &path, std::ios_base::openmode mode = std::ios_base::out)
    {
        auto f = std::make_unique<std::ofstream>();
        if (f->rdbuf()->open(path, mode) == nullptr)
            throw std::runtime_error(std::string("error opening file ") + path.string());
        f->exceptions(std::ios::failbit | std::ios::badbit);
        return f;
    }
}

appmap::config & appmap::config::instance()
{
    static appmap::config c = load_default();
    return c;
}

bool appmap::config::should_instrument(clrie::method_info method)
{
    const auto module = method.module_info();
    const std::string module_name = module.get(&IModuleInfo::GetModuleName);

    if (std::find(modules.begin(), modules.end(), module_name) != modules.end())
        return true;

    const fs::path module_path = std::string(module.get(&IModuleInfo::GetFullPath));

    for (fs::path &p: paths) {
        if (p.is_relative())
            p = resolve(p, base_path);
        if (const auto &[end, _] = std::mismatch(p.begin(), p.end(), module_path.begin()); end == p.end())
            return true;
    }

    const auto full_name = method.full_name();

    for (const auto &cls: classes) {
        if (cls.length() > full_name.length()) continue;
        if (full_name.rfind(cls, 0) == 0 && (
            full_name.length() == cls.length() ||
            full_name[cls.length()] == '.'
        ))
            return true;
    }

    return false;
}

std::filesystem::path appmap::config::appmap_output_dir() const noexcept
{
    if (!output_dir) {
        output_dir = get_envar("APPMAP_OUTPUT_DIR").value_or(base_path / "tmp" / "appmap");
        fs::create_directories(*output_dir);
    }

    return *output_dir;
}

std::unique_ptr<std::ostream> appmap::config::appmap_output_stream() const
{
    if (!appmap_output_path) return {};

    return output_file(*appmap_output_path);
}

std::unique_ptr<std::ostream> appmap::config::module_list_stream() const
{
    if (!module_list_path) return {};

    return output_file(*module_list_path);
}

std::pair<std::unique_ptr<std::ostream>, std::filesystem::path> appmap::config::appmap_output_stream(const std::string& name) const
{
    const auto outpath = appmap_output_dir() / (sanitize_filename(name) + ".appmap.json");
    return { output_file(outpath), outpath };
}

#ifndef DOCTEST_CONFIG_DISABLE
#include <fakeit.hpp>
using namespace fakeit;

REGISTER_EXCEPTION_TRANSLATOR(fakeit::FakeitException &ex) {
    return ex.what().c_str();
}

template <typename... Ts>
struct select_last
{
    template<typename T>
    struct tag
    {
        using type = T;
    };

    // Use a fold-expression to fold the comma operator over the parameter pack.
    using type = typename decltype((tag<Ts>{}, ...))::type;
};

template <typename C>
struct ComMock : public Mock<C>
{
    ComMock() {
        Fake(Method((*this), AddRef), Method((*this), Release));
        Method((*this), QueryInterface) = &this->get();
    }

    template <typename... arglist>
    struct ComMockingContext : public MockingContext<HRESULT, arglist...>
    {
        ComMockingContext(MockingContext<HRESULT, arglist...> &&mc)
        : MockingContext<HRESULT, arglist...>(std::move(mc)) {}

        using result_type = std::remove_pointer_t<typename select_last<arglist...>::type>;

        void operator=(const result_type &r) {
            auto method = [r](auto &&...args) {
                *std::get<sizeof...(arglist) - 1>(std::make_tuple(args...)) = r;
                return S_OK;
            };
            MethodMockingContext<HRESULT, arglist...>::setMethodBodyByAssignment(method);
        }

        void operator=(const char *r) {
            auto method = [str = utf8::utf8to16(r)](auto &&...args) {
                *std::get<sizeof...(arglist) - 1>(std::make_tuple(args...)) = SysAllocString(str.data());
                return S_OK;
            };
            MethodMockingContext<HRESULT, arglist...>::setMethodBodyByAssignment(method);
        }

        ComMockingContext<arglist...> &setMethodDetails(std::string mockName, std::string methodName) {
            MethodMockingContext<HRESULT, arglist...>::setMethodDetails(mockName, methodName);
            return *this;
        }
    };

    template <int id, typename R, typename T, typename... arglist>
    requires std::is_base_of_v<T, C>
    MockingContext<R, arglist...> stub(R(T::*vMethod)(arglist...)) {
        return this->Mock<C>::template stub<id>(vMethod);
    }

    template <int id, typename T, typename... arglist>
    requires std::is_base_of_v<T, C> && std::is_pointer_v<typename select_last<arglist...>::type>
    ComMockingContext<arglist...> stub(HRESULT(T::*vMethod)(arglist...)) {
        return this->Mock<C>::template stub<id>(vMethod);
    }
};

TEST_CASE("method matching")
{
    config c;

    ComMock<IModuleInfo> module;
    ComMock<IMethodInfo> method;
    Method(method, GetModuleInfo) = &module.get();
    Method(module, GetModuleName) = "xr.dll";
    Method(module, GetFullPath) = "/src/xr/bin/xr.dll";
    Method(method, GetFullName) = "Extinction.Rebellion.Protest";

    SUBCASE("by module") {
        load_config(c, YAML::Load("packages: [module: xr.dll]"));

        CHECK(c.should_instrument(&method.get()));

        Method(module, GetModuleName) = "other.dll";
        CHECK(not c.should_instrument(&method.get()));
    }

    SUBCASE("by class") {
        SUBCASE("with qualifier") {
            load_config(c, YAML::Load("packages: [class: Extinction.Rebellion]"));
        }

        SUBCASE("naked") {
            load_config(c, YAML::Load("packages: [Extinction.Rebellion]"));
        }

        CHECK(c.should_instrument(&method.get()));

        Method(method, GetFullName) = "Extinction.Rebellions.Protest";

        CHECK(not c.should_instrument(&method.get()));
    }

    SUBCASE("by path") {
        SUBCASE("absolute") {
            load_config(c, YAML::Load("packages: [path: /src/xr]"));
        }

        SUBCASE("relative") {
            load_config(c, YAML::Load("packages: [path: .]"));
            c.base_path = "/src/xr";
        }

        CHECK(c.should_instrument(&method.get()));

        Method(module, GetFullPath) = "/usr/share/xr.dll";

        CHECK(not c.should_instrument(&method.get()));
    }
}

#endif // testing enabled
