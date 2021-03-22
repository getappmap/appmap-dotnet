#pragma once

#include <com/ptr.h>

#include <initializer_list>
#include <memory>
#include <unordered_map>
#include <variant>

class IMethodInfo;

namespace appmap {
    namespace classmap {
        struct klass;
        struct package;
        struct function { bool is_static; };

        using code_object = std::variant<klass, package, function>;
        using code_object_ptr = std::unique_ptr<code_object>;

        using code_container_base = std::unordered_map<std::string, code_object_ptr>;
        struct code_container : code_container_base
        {
            using code_container_base::code_container_base;
            code_container(const code_container &other);
            code_container(std::initializer_list<std::pair<std::string, code_object>> init);
        };
        struct klass : code_container {
            using code_container::code_container;
        };
        struct package : code_container {
            using code_container::code_container;
        };
        struct classmap : code_container {
            using code_container::code_container;

            void add(com::ptr<IMethodInfo>);
        };
    }

    inline classmap::classmap class_map;
}
