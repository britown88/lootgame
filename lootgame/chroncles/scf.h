#pragma once

// Sean's Cool Format

#include "defs.h"

typedef struct SCFHeader SCFHeader;

enum SCFType_ {
   SCFType_NULL = 0,   
   SCFType_INT,
   SCFType_FLOAT,
   SCFType_DOUBLE,
   SCFType_STRING,
   SCFType_BYTES,
   SCFType_SUBLIST
};
typedef byte SCFType;

struct SCFReader {
   SCFHeader* header = nullptr;
   SCFType* typeList = nullptr;
   void* pos = nullptr;
};

SCFReader scfView(void const* scf);
bool scfReaderNull(SCFReader const& view);
bool scfReaderAtEnd(SCFReader const& view);

SCFType scfReaderPeek(SCFReader const& view);
uint64_t scfReaderRemaining(SCFReader const& view);
void scfReaderSkip(SCFReader& view);

SCFReader scfReadList(SCFReader& view);
int64_t const* scfReadInt(SCFReader& view);
float const* scfReadFloat(SCFReader& view);
double const* scfReadDouble(SCFReader& view);
StringView scfReadString(SCFReader& view);
byte const* scfReadBytes(SCFReader& view, uint64_t* sizeOut);

typedef struct SCFWriter SCFWriter;

SCFWriter* scfWriterCreate();
void scfWriterDestroy(SCFWriter* writer);

void scfWriteListBegin(SCFWriter* writer);
void scfWriteListEnd(SCFWriter* writer);
void scfWriteInt(SCFWriter* writer, int64_t i);
void scfWriteFloat(SCFWriter* writer, float f);
void scfWriteDouble(SCFWriter* writer, double d);
void scfWriteString(SCFWriter* writer, StringView string);
void scfWriteBytes(SCFWriter* writer, void const* data, uint64_t size);

void* scfWriteToBuffer(SCFWriter* writer, uint64_t* sizeOut = nullptr);

void DEBUG_imShowWriterStats(SCFWriter *writer);