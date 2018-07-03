#include "scf.h"

#include <string>
#include <vector>

static const u32 SCF_MAGIC_NUMBER = 373285619;

struct SCFHeader {
   const u32 magic = SCF_MAGIC_NUMBER;
   u32 binarySegmentOffset = 0;
};


static u32 _roundUp(u32 in) {
   return (in + 3) & ~3;
}

//get the number of bytes past the typelist for the start of the data
// stay 4-byte aligned so leave a little extra data
static u32 _dataOffset(SCFType const* typeList) {
   auto len = (u32)strlen((StringView)typeList);
   return _roundUp(len + 1);

   //_roundUp(tlist.size + 1) - (tlist.size);
}

static u32 _currentTypeSize(SCFReader const& view) {
   switch (*view.typeList) {
   case SCFType_NULL: return 0;
   case SCFType_INT: return sizeof(u32);
   case SCFType_FLOAT: return sizeof(f32);
   case SCFType_STRING: return sizeof(u32);
   case SCFType_BYTES: return sizeof(u32);
   case SCFType_SUBLIST: return *(u32*)view.pos;
   }

   return 0;
}

SCFReader scfView(void const* scf) {
   if (!scf) {
      return {};
   }

   auto header = (SCFHeader*)scf;
   if (header->magic != SCF_MAGIC_NUMBER) {
      return {};
   }

   SCFReader out;
   out.header = header;
   out.typeList = (SCFType*)(((byte*)scf) + sizeof(SCFHeader));
   out.pos = (byte*)out.typeList + _dataOffset(out.typeList);

   return out;
}
bool scfReaderNull(SCFReader const& view) {
   return !view.header;
}
bool scfReaderAtEnd(SCFReader const& view) {
   return *view.typeList == SCFType_NULL;
}

SCFType scfReaderPeek(SCFReader const& view) {
   return *view.typeList;
}
u32 scfReaderRemaining(SCFReader const& view) {
   return (u32)strlen((StringView)view.typeList);
}
void scfReaderSkip(SCFReader& view) {   
   (byte*&)view.pos += _currentTypeSize(view);
   ++view.typeList;
}

SCFReader scfReadList(SCFReader& view) {
   if (*view.typeList != SCFType_SUBLIST) { return {};  }

   u32 listSize = *(u32*)view.pos;

   SCFReader out;
   out.header = view.header;
   out.typeList = (byte*)view.pos + sizeof(u32);
   out.pos = (byte*)out.typeList + _dataOffset(out.typeList);

   (byte*&)view.pos += sizeof(u32) + listSize;
   ++view.typeList;

   return out;
}
i32 const* scfReadInt(SCFReader& view) {
   if (*view.typeList != SCFType_INT) { return nullptr; }
   auto out = (i32*)view.pos;
   scfReaderSkip(view);
   return out;
}
f32 const* scfReadFloat(SCFReader& view) {
   if (*view.typeList != SCFType_FLOAT) { return nullptr; }
   auto out = (f32*)view.pos;
   scfReaderSkip(view);
   return out;
}
StringView scfReadString(SCFReader& view) {
   if (*view.typeList != SCFType_STRING) { return nullptr; }
   auto offset = *(u32*)view.pos;
   scfReaderSkip(view);
   return (StringView)((byte*)view.header + view.header->binarySegmentOffset + offset);
}
byte const* scfReadBytes(SCFReader& view, u32* sizeOut) {
   if (*view.typeList != SCFType_BYTES) { return nullptr; }
   auto offset = *(u32*)view.pos;
   scfReaderSkip(view);

   auto bin = (byte*)view.header + view.header->binarySegmentOffset + offset;
   *sizeOut = *(u32*)bin;
   return bin + sizeof(u32);
}

struct SCFBuffer {
   byte* data = nullptr;
   u32 size = 0, capacity = 0;

   void grow(u32 count) {
      if (capacity < size + count) {
         capacity = (size+count) * 2;
         byte* newBuff = new byte[capacity];
         memcpy(newBuff, data, size);
         delete[] data;
         data = newBuff;
      }
   }

   void push(byte b) {
      grow(1);
      data[size++] = b;
   }
   void push(byte*buff, u32 len) {
      grow(len);
      memcpy(data + size, buff, len);
      size += len;
   }
};

struct SCFWriter {
   std::vector<SCFBuffer> currentTypeList;
   std::vector<SCFBuffer> currentDataSet;

   SCFBuffer dataSegment;
   SCFBuffer binarySegment;
};

static StringView _typeName(SCFType type) {
   switch (type) {
   case SCFType_NULL: return "Null";
   case SCFType_INT: return "Int";
   case SCFType_FLOAT: return "Float";
   case SCFType_STRING: return "String";
   case SCFType_BYTES: return "Bytes";
   case SCFType_SUBLIST: return "Sublist";
   }
   return "Unknown";
}
#include <imgui.h>
void DEBUG_imShowWriterStats(SCFWriter *writer) {
   ImGui::Text("List stack size: %d", writer->currentTypeList.size());

   if (ImGui::TreeNode("Current Type List")) {
      auto &l = writer->currentTypeList.back();
      for (u32 i = 0; i < l.size; ++i) {
         ImGui::Text(_typeName(l.data[i]));
      }
      ImGui::TreePop();
   }

   ImGui::Text("Current List Dataset Size: %d", writer->currentDataSet.back().size);
   ImGui::Text("Current Binary Segment Size: %d", writer->binarySegment.size);
}

SCFWriter* scfWriterCreate() {
   auto out = new SCFWriter();
   out->currentTypeList.push_back({});
   out->currentDataSet.push_back({});
   return out;
}
void scfWriterDestroy(SCFWriter* writer) {

   for (auto &l : writer->currentTypeList) {
      if (l.data) { delete[] l.data; }
   }

   for (auto &l : writer->currentDataSet) {
      if (l.data) { delete[] l.data; }
   }

   delete writer;
}

void scfWriteListBegin(SCFWriter* writer) {
   writer->currentTypeList.back().push(SCFType_SUBLIST);
   writer->currentTypeList.push_back({});
   writer->currentDataSet.push_back({});
}
void scfWriteListEnd(SCFWriter* writer) {
   if (writer->currentTypeList.size() <= 1) {
      return;
   }

   auto &tlist = writer->currentTypeList.back();
   auto &dSet = writer->currentDataSet.back();

   int padding = _roundUp(tlist.size + 1) - (tlist.size);
   u32 listSize = tlist.size + padding + dSet.size;

   //we push our data onto the parents dataset
   auto &parentSet = *(writer->currentDataSet.end() - 2);
   parentSet.grow(sizeof(listSize) + listSize); // small optimization for a large growth
   parentSet.push((byte*)&listSize, sizeof(listSize)); //push list size
   parentSet.push((byte*)tlist.data, tlist.size); //push typelist
   parentSet.push((byte*)"\0\0\0\0", padding); //push padding
   parentSet.push((byte*)dSet.data, dSet.size); //push dataset

   if (tlist.data) { delete[] tlist.data; }
   if (dSet.data) { delete[] dSet.data; }

   writer->currentTypeList.erase(writer->currentTypeList.end() - 1);
   writer->currentDataSet.erase(writer->currentDataSet.end() - 1);
}
void scfWriteInt(SCFWriter* writer, i32 i) {
   auto &tlist = writer->currentTypeList.back();
   auto &dSet = writer->currentDataSet.back();

   tlist.push(SCFType_INT);
   dSet.push((byte*)&i, sizeof(i));
}
void scfWriteFloat(SCFWriter* writer, f32 f) {
   auto &tlist = writer->currentTypeList.back();
   auto &dSet = writer->currentDataSet.back();

   tlist.push(SCFType_FLOAT);
   dSet.push((byte*)&f, sizeof(f));
}
void scfWriteString(SCFWriter* writer, StringView string) {
   u32 len = (u32)strlen(string) + 1;
   u32 offset = writer->binarySegment.size;

   writer->binarySegment.push((byte*)string, len); //push to binary segment

   auto &tlist = writer->currentTypeList.back();
   auto &dSet = writer->currentDataSet.back();
   tlist.push(SCFType_STRING);
   dSet.push((byte*)&offset, sizeof(offset)); // push binary offset into dataset
}
void scfWriteBytes(SCFWriter* writer, void const* data, u32 size) {
   u32 offset = writer->binarySegment.size;

   writer->binarySegment.push((byte*)&size, sizeof(size)); //push size value to binary
   writer->binarySegment.push((byte*)data, size); //push to binary segment

   auto &tlist = writer->currentTypeList.back();
   auto &dSet = writer->currentDataSet.back();
   tlist.push(SCFType_BYTES);
   dSet.push((byte*)&offset, sizeof(offset)); // push binary offset into dataset
}

void* scfWriteToBuffer(SCFWriter* writer, u32* sizeOut) {

   //first copy current list to main set
   auto &tlist = writer->currentTypeList.back();
   auto &dSet = writer->currentDataSet.back();
   int padding = _roundUp(tlist.size + 1) - (tlist.size);

   u32 listSize = tlist.size + padding + dSet.size;

   writer->dataSegment.grow( listSize); // small optimization for a large growth
   writer->dataSegment.push((byte*)tlist.data, tlist.size); //push typelist
   writer->dataSegment.push((byte*)"\0\0\0\0", padding); //push padding
   writer->dataSegment.push((byte*)dSet.data, dSet.size); //push dataset

   auto dataSize = writer->dataSegment.size;
   auto binarySize = writer->binarySegment.size;

   u32 totalSize = sizeof(SCFHeader) + dataSize + binarySize;

   byte* out = new byte[totalSize];
   memset(out, 0, totalSize);

   SCFHeader header;
   header.binarySegmentOffset = sizeof(SCFHeader) + dataSize;

   memcpy(out, (byte*)&header, sizeof(header));
   memcpy(out + sizeof(header), writer->dataSegment.data, dataSize);
   memcpy(out + header.binarySegmentOffset, writer->binarySegment.data, binarySize);

   *sizeOut = totalSize;
   return out;
}