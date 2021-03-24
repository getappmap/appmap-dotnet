#include <palrt.h>

void SysFreeString(BSTR bstr)
{
    if (bstr == NULL)
        return;
    free(reinterpret_cast<char **>(bstr) - 1);
}

BSTR SysAllocString(const char16_t *str)
{
    const char16_t *end = str;
    for (; *end; end++);

    BSTR res = reinterpret_cast<char16_t *>(reinterpret_cast<char **>(malloc((end - str + 1) * 2 + sizeof(char *))) + 1);
    *(reinterpret_cast<uint32_t *>(res) - 1) = end - str;

    char16_t *rend = res;
    for (rend = res; *str; str++)
        *(rend++) = *str;

    *rend = 0;

    return res;
}
