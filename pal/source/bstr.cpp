#include <palrt.h>

void SysFreeString(BSTR bstr)
{
    if (bstr == NULL)
        return;
    free(reinterpret_cast<char *>(bstr) - sizeof(char *));
}
