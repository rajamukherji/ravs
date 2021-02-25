#include "ravs.h"
#include "radb/radb.h"
#include "bsdiff.h"
#include "bspatch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef RADB_MEM_GC
#include <gc/gc.h>
#endif

typedef struct block_t block_t;
typedef struct change_t change_t;

struct version_store_t {
	fixed_store_t *Blocks;
	string_store_t *Values;
	string_store_t *Changes;
	string_index_t *Authors;
	string_store_writer_t Writer[1];
	uint32_t Change;
};

struct block_t {
	uint32_t Next;
	uint32_t Change;
	uint32_t Length;
	uint32_t Value;
};

struct change_t {
	time_t Time;
	uint32_t Author;
};

version_store_t *version_store_create(const char *Prefix, size_t RequestedSize RADB_MEM_PARAMS) {
#if defined(RADB_MEM_MALLOC)
	version_store_t *Store = malloc(sizeof(version_store_t));
#elif defined(RADB_MEM_GC)
	version_store_t *Store = GC_malloc(sizeof(version_store_t));
#else
	version_store_t *Store = alloc(Allocator, sizeof(version_store_t));
	Store->Allocator = Allocator;
	Store->alloc = alloc;
	Store->alloc_atomic = alloc_atomic;
	Store->free = free;
#endif
	char StorePrefix[strlen(Prefix) + 10];
	sprintf(StorePrefix, "%s_blocks", Prefix);
	Store->Blocks = fixed_store_create(StorePrefix, 16, 0 RADB_MEM_ARGS);
	sprintf(StorePrefix, "%s_values", Prefix);
	Store->Values = string_store_create(StorePrefix, RequestedSize, 0 RADB_MEM_ARGS);
	sprintf(StorePrefix, "%s_changes", Prefix);
	Store->Changes = string_store_create(StorePrefix, 16, 0 RADB_MEM_ARGS);
	sprintf(StorePrefix, "%s_authors", Prefix);
	Store->Authors = string_index_create(StorePrefix, 16, 0 RADB_MEM_ARGS);
	Store->Change = INVALID_INDEX;
	return Store;
}

version_store_t *version_store_open(const char *Prefix RADB_MEM_PARAMS) {
#if defined(RADB_MEM_MALLOC)
	version_store_t *Store = malloc(sizeof(version_store_t));
#elif defined(RADB_MEM_GC)
	version_store_t *Store = GC_malloc(sizeof(version_store_t));
#else
	version_store_t *Store = alloc(Allocator, sizeof(version_store_t));
	Store->Allocator = Allocator;
	Store->alloc = alloc;
	Store->alloc_atomic = alloc_atomic;
	Store->free = free;
#endif
	char StorePrefix[strlen(Prefix) + 10];
	sprintf(StorePrefix, "%s_blocks", Prefix);
	Store->Blocks = fixed_store_open(StorePrefix RADB_MEM_ARGS);
	sprintf(StorePrefix, "%s_values", Prefix);
	Store->Values = string_store_open(StorePrefix RADB_MEM_ARGS);
	sprintf(StorePrefix, "%s_changes", Prefix);
	Store->Changes = string_store_open(StorePrefix RADB_MEM_ARGS);
	sprintf(StorePrefix, "%s_authors", Prefix);
	Store->Authors = string_index_open(StorePrefix RADB_MEM_ARGS);
	Store->Change = INVALID_INDEX;
	return Store;
}

void version_store_close(version_store_t *Store) {
	fixed_store_close(Store->Blocks);
	string_store_close(Store->Values);
	string_store_close(Store->Changes);
#if defined(RADB_MEM_MALLOC)
	free(Store);
#elif defined(RADB_MEM_GC)
#else
	Store->free(Store->Allocator, Store);
#endif
}

void version_store_change_create(version_store_t *Store, const char *AuthorName, size_t AuthorLength) {
	Store->Change = string_store_alloc(Store->Changes);
	string_store_writer_open(Store->Writer, Store->Changes, Store->Change);
	change_t Change[1];
	Change->Time = time(NULL);
	Change->Author = string_index_insert(Store->Authors, AuthorName, AuthorLength);
	string_store_writer_write(Store->Writer, Change, sizeof(change_t));
}

size_t version_store_value_create(version_store_t *Store, void *Bytes, size_t Length) {
	uint32_t Index = fixed_store_alloc(Store->Blocks);
	string_store_writer_write(Store->Writer, &Index, 4);
	block_t *Block = fixed_store_get(Store->Blocks, Index);
	Block->Change = Store->Change;
	Block->Next = INVALID_INDEX;
	Block->Length = Length;
	Block->Value = string_store_alloc(Store->Values);
	string_store_set(Store->Values, Block->Value, Bytes, Length);
	return Index;
}

static int diff_write(struct bsdiff_stream *Stream, const void *Buffer, int Size) {
	string_store_writer_write((string_store_writer_t *)Stream->opaque, Buffer, Size);
	return 0;
}

void version_store_value_update(version_store_t *Store, size_t Index, void *Bytes, size_t Length) {
	uint32_t DiffIndex = fixed_store_alloc(Store->Blocks);
	block_t *Block = fixed_store_get(Store->Blocks, Index);
	size_t OldLength = string_store_size(Store->Values, Block->Value);
	void *OldBytes = alloca(OldLength);
	string_store_get(Store->Values, Block->Value, OldBytes, OldLength);
	block_t *Diff = fixed_store_get(Store->Blocks, DiffIndex);
	Diff->Change = Block->Change;
	Diff->Length = OldLength;
	Diff->Next = Block->Next;
	Diff->Value = string_store_alloc(Store->Values);
	string_store_writer_t DiffWriter[1];
	string_store_writer_open(DiffWriter, Store->Values, Diff->Value);
	struct bsdiff_stream DiffStream[1];
	DiffStream->opaque = DiffWriter;
	DiffStream->malloc = malloc;
	DiffStream->free = free;
	DiffStream->write = diff_write;
	bsdiff(Bytes, Length, OldBytes, OldLength, DiffStream);
	Block->Change = Store->Change;
	Block->Next = DiffIndex;
	if (Block->Length < Length) Block->Length = Length;
	string_store_set(Store->Values, Block->Value, Bytes, Length);
}

void version_store_value_erase(version_store_t *Store, size_t Index) {

}

size_t version_store_value_size(version_store_t *Store, size_t Index) {
	block_t *Block = fixed_store_get(Store->Blocks, Index);
	return string_store_size(Store->Values, Block->Value);
}

size_t version_store_value_get(version_store_t *Store, size_t Index, void *Buffer, size_t Space) {
	block_t *Block = fixed_store_get(Store->Blocks, Index);
	return string_store_get(Store->Values, Block->Value, Buffer, Space);
}

void version_store_value_history(version_store_t *Store, size_t Index, int (*Callback)(void *Data, uint32_t Change, time_t Time, uint32_t Author), void *Data) {
	block_t *Block = fixed_store_get(Store->Blocks, Index);
	change_t Change[1];
	string_store_get(Store->Changes, Block->Change, Change, sizeof(change_t));
	Callback(Data, Block->Change, Change->Time, Change->Author);
	while ((Index = Block->Next) != INVALID_INDEX) {
		Block = fixed_store_get(Store->Blocks, Index);
		string_store_get(Store->Changes, Block->Change, Change, sizeof(change_t));
		Callback(Data, Block->Change, Change->Time, Change->Author);
	}
}

size_t version_store_value_revision_size(version_store_t *Store, size_t Index, size_t Change) {
	block_t *Block = fixed_store_get(Store->Blocks, Index);
	if (Block->Change <= Change) return string_store_size(Store->Values, Block->Value);
	while ((Index = Block->Next) != INVALID_INDEX) {
		Block = fixed_store_get(Store->Blocks, Index);
		if (Block->Change <= Change) return Block->Length;
	}
	return 0;
}

static int patch_read(const struct bspatch_stream *Stream, void *Buffer, int Size) {
	string_store_reader_read((string_store_reader_t *)Stream->opaque, Buffer, Size);
	return 0;
}

size_t version_store_value_revision_get(version_store_t *Store, size_t Index, size_t Change, void *Bytes, size_t Space) {
	block_t *Block = fixed_store_get(Store->Blocks, Index);
	if (Block->Change <= Change) return string_store_get(Store->Values, Block->Value, Bytes, Space);
	void *Old = alloca(Block->Length);
	void *New = alloca(Block->Length);
	size_t OldSize = string_store_get(Store->Values, Block->Value, Bytes, Block->Length);
	while ((Index = Block->Next) != INVALID_INDEX) {
		Block = fixed_store_get(Store->Blocks, Index);
		string_store_reader_t PatchReader[1];
		string_store_reader_open(PatchReader, Store->Values, Block->Value);
		struct bspatch_stream PatchStream[1];
		PatchStream->opaque = PatchReader;
		PatchStream->read = patch_read;
		bspatch(Old, OldSize, New, Block->Length, PatchStream);
		OldSize = Block->Length;
		void *Temp = New;
		New = Old;
		Old = Temp;
		if (Block->Change <= Change) break;
	}
	memcpy(Bytes, Old, OldSize);
	return OldSize;
}
