#ifndef __ASSERT_H__
#define __ASSERT_H__

void assertion_failure(char *exp, char *file, char *base, int line);

#define assert(exp) \
    if (exp)        \
        ;           \
    else            \
        assertion_failure(#exp, __FILE__, __BASE_FILE__, __LINE__)

#endif