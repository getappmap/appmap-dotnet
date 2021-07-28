#pragma once

#include <com/ptr.h>

#include <initializer_list>
#include <memory>
#include <unordered_map>
#include <variant>

struct IMethodInfo;

namespace appmap {
    namespace classmap {
        struct code_container;
        struct function { bool is_static; };

        using code_object = std::variant<code_container, function>;
        using code_object_ptr = std::unique_ptr<code_object>;

        using code_container_base = std::unordered_map<std::string, code_object_ptr>;
        struct code_container : code_container_base
        {
            enum container_kind { classmap, klass, package };
            container_kind kind = classmap;

            using code_container_base::code_container_base;
        };

        using classmap = code_container;
    }
}
