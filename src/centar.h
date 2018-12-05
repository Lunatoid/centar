//
// Do this:
//   #define CENTAR_IMPLEMENTATION
// before you include this file in *one* C or C++ file to create the implementation.
//
//

#ifndef CENTAR_H_
#define CENTAR_H_

#include <stdint.h>
#include <time.h>
#include <stdio.h>

#ifndef CENTAR_MALLOC
#  define CENTAR_MALLOC(size) malloc(size)
#endif

#ifndef CENTAR_FREE
#  define CENTAR_FREE(ptr) free(ptr)
#endif

// If in C++ define CENTAR_BOOL as bool, if in C define CENTAR_BOOL as uint8_t
#ifndef CENTAR_BOOL
#  ifndef __cplusplus
#    define CENTAR_BOOL uint8_t
#  else
#    define CENTAR_BOOL bool
#  endif
#endif


#ifdef CENTAR_STATIC
#  ifdef __cplusplus
#    define CTAR_API extern "C" static
#  else
#    define CTAR_API static
#  endif
#else
#  ifdef __cplusplus
#    define CTAR_API extern "C"
#  else
#    define CTAR_API extern
#  endif
#endif

typedef struct TarHeader {
  char name[100];
  uint64_t file_size;
  time_t last_modified;
  
  uint64_t position;
  struct TarHeader* next;
} TarHeader;

typedef struct Tar {
  char path[512];
  TarHeader* header;
} Tar;

// Parses a tar file
CTAR_API CENTAR_BOOL ctar_parse(const char* path, Tar* out);

// Frees a tar file
CTAR_API void ctar_free(Tar* tar);

// Finds a header of a desired file
// Returns a nullptr if none was found
CTAR_API TarHeader* ctar_find(Tar* tar, const char* name);

// Allocates a block of memory and writes the file from the tar archive into it
CTAR_API char* ctar_read(Tar* tar, const char* name,
                         long int* file_size, CENTAR_BOOL null_terminated);

// Writes the file from the tar archive into a block of memory
CTAR_API void ctar_read_into(Tar* tar, const char* name, char* memory,
                             long int* file_size, CENTAR_BOOL null_terminated);

// Prepare for writing files
CTAR_API FILE* ctar_begin_write(const char* path);

// Write a file
CTAR_API CENTAR_BOOL ctar_write(FILE* file, const char* name, char* memory, long int memory_size); 

// Stop writing
CTAR_API void ctar_end_write(FILE* file);

#endif // CENTAR_H_

#ifdef CENTAR_IMPLEMENTATION
#undef CENTAR_IMPLEMENTATION

#include <string.h>
#include <stdlib.h>

typedef struct TarHeaderRaw {
  int8_t name[100];
  int8_t mode[8];
  int8_t owner_id[8];
  int8_t group_id[8];
  int8_t file_size[12];
  int8_t last_modified[12];
  int8_t checksum[8];
  int8_t link_indicator;
  int8_t linked_name[100];
  int8_t padding[255];
} TarHeaderRaw;

const char CENTAR_NULL[512] = {0};

static long int ctar_round_up(long int to_round, long int multiple) {
  if (multiple == 0) return to_round;
  
  long int remainder = to_round % multiple;
  if (remainder == 0) return to_round;
  
  return to_round + multiple - remainder;
}

CTAR_API TarHeader ctar_parse_raw(TarHeaderRaw* raw) {
  TarHeader header = {0};
  strcpy((char*)header.name, (char*)raw->name);
  header.file_size = strtol((const char*)raw->file_size, NULL, 8);
  header.last_modified = strtol((const char*)raw->last_modified, NULL, 8);
  
  return header;
};

CTAR_API CENTAR_BOOL ctar_parse(const char* path, Tar* out) {
  FILE* file = fopen(path, "rb");
  
  if (!file) {
    printf("Error opening file '%s'\n", path);
    return 0;
  }
  
  TarHeaderRaw raw;
  
  // Parse initial header
  fread(&raw, sizeof(raw), 1, file);
  
  if (memcmp(&raw, CENTAR_NULL, 512) == 0) return 0;
  
  strcpy(out->path, path);
  
  TarHeader* header = (TarHeader*)CENTAR_MALLOC(sizeof(TarHeader));
  *header = ctar_parse_raw(&raw);
  header->position = ftell(file);
  
  out->header = header;
  
  // Skip to next header
  fseek(file, ctar_round_up(header->file_size, 512), SEEK_CUR);
  
  TarHeader* previous_entry = header;
  
  // Parse all headers
  while (!feof(file)) {
    fread(&raw, sizeof(raw), 1, file);
    
    if (memcmp(&raw, CENTAR_NULL, 512) == 0) break;
    
    TarHeader* current_entry = (TarHeader*)CENTAR_MALLOC(sizeof(TarHeader));
    previous_entry->next = current_entry;
    
    *current_entry = ctar_parse_raw(&raw);
    current_entry->position = ftell(file);
    
    previous_entry = current_entry;
    
    // Skip to next header
    fseek(file, ctar_round_up(current_entry->file_size, 512), SEEK_CUR);
  }
  
  previous_entry->next = NULL;
  
  fclose(file);
  
  return 1;
}

CTAR_API void ctar_free(Tar* tar) {
  if (!tar) return;
  
  // Free all headers
  TarHeader* head = tar->header;
  TarHeader* tmp;
  while (head) {
    tmp = head;
    head = head->next;
    CENTAR_FREE(tmp);
  }
}

CTAR_API TarHeader* ctar_find(Tar* tar, const char* name) {
  if (!tar || !name) return NULL;
  
  for (TarHeader* t = tar->header; t != NULL; t = t->next) {
    if (strcmp(t->name, name) == 0) {
      return t;
    }
  }
  
  return NULL;
}

CTAR_API char* ctar_read(Tar* tar, const char* name,
                         long int* file_size, CENTAR_BOOL null_terminated) {
  if (!tar || !name) return NULL;
  
  TarHeader* t = ctar_find(tar, name);
  
  if (!t) return NULL;
  
  uint64_t memory_size = t->file_size + ((null_terminated != 0) ? 1 : 0);
  
  char* memory = (char*)CENTAR_MALLOC(memory_size);
  
  // Read into created memory block
  ctar_read_into(tar, name, memory, file_size, null_terminated);
  
  return memory;
}

CTAR_API void ctar_read_into(Tar* tar, const char* name, char* memory,
                             long int* file_size, CENTAR_BOOL null_terminated) {
  if (!tar || !name || !memory) return;
  
  TarHeader* t = ctar_find(tar, name);
  
  if (!t) return;
  
  if (file_size) {
    *file_size = t->file_size + ((null_terminated != 0) ? 1 : 0);
  }
  
  FILE* file = fopen(tar->path, "rb");
  
  if (!file) return;
  
  fseek(file, t->position, SEEK_SET);
  fread(memory, t->file_size, 1, file);
  
  fclose(file);
  
  if (null_terminated != 0) {
    memory[t->file_size] = '\0';
  }
}

CTAR_API FILE* ctar_begin_write(const char* path) {
  if (!path) return NULL;
  
  FILE* file = fopen(path, "wb");
  
  return file;
}

CTAR_API CENTAR_BOOL ctar_write(FILE* file, const char* name, char* memory, long int memory_size) {
  if (!file || !name || !memory) return 0;
  
  TarHeaderRaw header = {0};
  
  // Fill out each relevant field
  strcpy((char*)header.name, name);
  sprintf((char*)header.mode, "%7o", 666);
  sprintf((char*)header.file_size, "%11o", memory_size);
  sprintf((char*)header.last_modified, "%11o", (unsigned int)time(NULL));
  
  // Calculate checksum, do this last
  const uint8_t* header_bytes = (const uint8_t*)&header;
  int checksum = 256;
  for (int i = 0; i < sizeof(header); ++i) {
    checksum += header_bytes[i];
  }
  
  sprintf((char*)header.checksum, "%06o ", checksum);
  
  // Write header
  fwrite(&header, sizeof(header), 1, file);
  
  // Write data
  fwrite(memory, memory_size, 1, file);
  
  // Write data padding
  long int padding_size = ctar_round_up(memory_size, 512) - memory_size;
  
  const char* NULL_BYTE = "\0";
  for (int i = 0; i < padding_size; ++i) {
    fwrite(NULL_BYTE, 1, 1, file);
  }
  
  return 1;
}

CTAR_API void ctar_end_write(FILE* file) {
  if (!file) return;
  
  // Write null header
  fwrite(CENTAR_NULL, 512, 1, file);
  fclose(file);
}

#endif // CENTAR_IMPLEMENTATION
