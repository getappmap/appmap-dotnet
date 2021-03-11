#include "method.h"
#include <fstream>

using namespace appmap;

void appmap::instrumentation_method::on_module_loaded(clrie::module_info module)
{
    modules.insert(module.module_name());
}

void appmap::instrumentation_method::on_shutdown()
{
    if (config.module_list_path) {
        std::ofstream f(*config.module_list_path, std::ios_base::app);
        for (const auto &mod : modules) {
            f << mod << '\n';
        }
    }
}
