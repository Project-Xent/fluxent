#ifndef FLUX_STR_H
#define FLUX_STR_H

char *flux_str_dup(char const *s);

void  flux_str_replace(char const **slot, char const *s);

void  flux_str_free(char const *s);

#endif
