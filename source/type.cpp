#include "type.h"

using namespace appmap;

std::string appmap::friendly_name(const clrie::type &t)
{
    switch (t.cor_element_type()) {
        case ELEMENT_TYPE_SZARRAY: {
            auto related = t.as<ICompositeType>().get(&ICompositeType::GetRelatedType);
            return friendly_name(std::move(related)) + "[]";
        }
        default:
            return t.name();
    }
}
