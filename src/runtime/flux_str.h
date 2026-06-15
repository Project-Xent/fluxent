/**
 * @file flux_str.h
 * @brief Owned-string helpers for control data.
 *
 * Control data owns its strings: creation paths copy caller strings with
 * flux_str_dup, setters swap them with flux_str_replace, and destructors
 * release them with flux_str_free. Callers may pass transient buffers
 * (stack, arena) to any flux_* API that takes a string.
 */
#ifndef FLUX_STR_H
#define FLUX_STR_H

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief NULL-safe strdup. Returns NULL for NULL input or allocation failure. */
char *flux_str_dup(char const *s);

/** @brief Free the old string in @p slot and store a copy of @p s (NULL clears). */
void  flux_str_replace(char const **slot, char const *s);

/** @brief NULL-safe free of an owned string previously created by flux_str_dup. */
void  flux_str_free(char const *s);

#ifdef __cplusplus
}
#endif

#endif
