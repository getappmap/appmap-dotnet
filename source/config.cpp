#include <algorithm>
#include <doctest/doctest.h>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include <utf8.h>

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

    std::optional<fs::path> find_file(const std::string name) {
        for (fs::path dir = fs::current_path(); dir != dir.root_path(); dir = dir.parent_path()) {
            const auto file = dir / name;
            if (fs::exists(file))
                return file;
        }

        spdlog::warn("no {} found in {}", name, fs::current_path().string());

        return std::nullopt;
    }

    std::optional<fs::path> config_file_path() {
        const auto from_env = get_envar("APPMAP_CONFIG");
        if (from_env)
            return from_env;
        else
            return find_file("appmap.yml");
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

        // it's probably not the best place for this, but it'll do
        if (const auto &log_level = get_envar("APPMAP_LOG_LEVEL"))
            spdlog::set_level(spdlog::level::from_str(*log_level));
        else
            spdlog::set_level(spdlog::level::info);

        if (const auto config_path = config_file_path())
            load_config(c, YAML::LoadFile(*config_path));

        return c;
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
    const std::string module_name = module.mut().get(&IModuleInfo::GetModuleName);

    if (std::find(modules.begin(), modules.end(), module_name) != modules.end())
        return true;

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

#ifndef DOCTEST_CONFIG_DISABLE
#include <fakeit.hpp>
using namespace fakeit;

REGISTER_EXCEPTION_TRANSLATOR(fakeit::FakeitException &ex) {
    return ex.what().c_str();
}

template<typename... Ts>
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

    template<int id, typename R, typename T, typename... arglist, class = typename std::enable_if<
            !std::is_void<R>::value && !std::is_same_v<R, HRESULT> && std::is_base_of<T, C>::value>::type>
    MockingContext<R, arglist...> stub(R(T::*vMethod)(arglist...)) {
        return this->Mock<C>::template stub<id>(vMethod);
    }

    template<typename... arglist>
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

        template <class = std::enable_if<std::is_same_v<result_type, BSTR>>>
        void operator=(const char *r) {
            auto method = [r](auto &&...args) {
                *std::get<sizeof...(arglist) - 1>(std::make_tuple(args...)) = SysAllocString(utf8::utf8to16(r).data());
                return S_OK;
            };
            MethodMockingContext<HRESULT, arglist...>::setMethodBodyByAssignment(method);
        }

        ComMockingContext<arglist...> &setMethodDetails(std::string mockName, std::string methodName) {
            MethodMockingContext<HRESULT, arglist...>::setMethodDetails(mockName, methodName);
            return *this;
        }
    };

    template<int id, typename T, typename... arglist, class = typename std::enable_if<
            std::is_base_of<T, C>::value && std::is_pointer_v<typename select_last<arglist...>::type>>::type>
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
    Method(module, GetModuleName) = "test.dll";
    Method(method, GetFullName) = "Extinction.Rebellion.Protest";

    SUBCASE("by module") {
        load_config(c, YAML::Load("packages: [module: test.dll]"));

        CHECK(c.should_instrument(&method.get()));

        Method(module, GetModuleName) = "other.dll";
        CHECK(!c.should_instrument(&method.get()));
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

        CHECK(!c.should_instrument(&method.get()));
    }
}

#endif // testing enabled
