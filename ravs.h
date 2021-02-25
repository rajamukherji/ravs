#ifndef RAVS_H
#define RAVS_H

#include "radb/config.h"
#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef struct version_store_t version_store_t;

version_store_t *version_store_create(const char *Prefix, size_t RequestedSize RADB_MEM_PARAMS);

version_store_t *version_store_open(const char *Prefix RADB_MEM_PARAMS);

void version_store_close(version_store_t *Store);

void version_store_change_create(version_store_t *Store, const char *AuthorName, size_t AuthorLength);

size_t version_store_value_create(version_store_t *Store, void *Bytes, size_t Length);

void version_store_value_update(version_store_t *Store, size_t Index, void *Bytes, size_t Length);

void version_store_value_erase(version_store_t *Store, size_t Index);

size_t version_store_value_size(version_store_t *Store, size_t Index);

size_t version_store_value_get(version_store_t *Store, size_t Index, void *Buffer, size_t Space);

void version_store_value_history(version_store_t *Store, size_t Index, int (*Callback)(void *Data, time_t Time, uint32_t Author), void *Data);

size_t version_store_value_revision_size(version_store_t *Store, size_t Index, size_t Change);

size_t version_store_value_revision_get(version_store_t *Store, size_t Index, size_t Change, void *Bytes, size_t Space);

#endif
