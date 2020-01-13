//
// Do this:
//   #define CENTAR_IMPLEMENTATION
// before you include this file in *one* C or C++ file to create the implementation.
//
// Example:
//  #include ...
//  #include ...
//  #include ...
//  #define CENTAR_IMPLEMENTATION
//  #include "centar.h"
//
// If you don't want to use malloc/free you can define #define CENTAR_MALLOC(size)/CENTAR_FREE(ptr).
//
// License:
//   See the end of file for license information
//
// Reading API:
//   To parse a tar file you can do this:
//
//   Tar tar;
//   CENTAR_BOOL success = ctar_parse("path/to/file.tar", &tar);
// 
//   It will return 0 on failure and 1 on success.
//   ctar_parse() will parse all the file headers in the file and store
//   them in a linked list.
//
//   You can enumerate over all the headers like so:
//
//   for (TarHeader* header = tar.header; header != NULL; header = header->next) {
//      ...
//   }
//
//   After you're done using the tar archive, you can free it with ctar_free(&tar)
//   To read data you can either use ctar_read(), which will allocate a block of memory
//   using CENTAR_MALLOC and reads the data into it.
//
//   You can also use ctar_read_into() to read the data into a block of memory, make sure
//   the block of memory is large enough to store the data.
//
//   To find a header, you can use ctar_find(), and to rename a file you can use ctar_rename()
//
// Writing API:
//   To write a tar file you can do this:
//
//   FILE* out = ctar_begin_write("path/to/file.tar");
//   
//   const char* some_data = "This is some text file that we're gonna write.";
//   ctar_write(out, "hello.txt", some_data, strlen(some_data));
//
//   ctar_end_write(out);
//
//   If you want to export an existing Tar variable, you can use ctar_export(&tar, "path/to/file.tar");
//
//   One thing to note is that you shouldn't export a tar file to the tar file that you opened.
//   ctar_export() will read the original archive and then read each file into memory and write it to the new file.
//

#ifndef CENTAR_H_
#define CENTAR_H_

#include <stdint.h>
#include <time.h>
#include <stdio.h>

// You can define your own CENTAR_MALLOC
#ifndef CENTAR_MALLOC
#  define CENTAR_MALLOC(size) malloc(size)
#endif

// You can define your own CENTAR_FREE
#ifndef CENTAR_FREE
#  define CENTAR_FREE(ptr) free(ptr)
#endif

// You can define your own CENTAR_BOOL.
// If in C++, define CENTAR_BOOL as bool, if in C, define CENTAR_BOOL as uint8_t
#ifndef CENTAR_BOOL
#  ifndef __cplusplus
#    define CENTAR_BOOL uint8_t
#  else
#    define CENTAR_BOOL bool
#  endif
#endif

// You can define your own CENTAR_API
#ifndef CENTAR_API
#  ifdef CENTAR_STATIC
#    ifdef __cplusplus
#      define CENTAR_API extern "C" static
#    else
#      define CENTAR_API static
#    endif
#  else
#    ifdef __cplusplus
#      define CENTAR_API extern "C"
#    else
#      define CENTAR_API extern
#    endif
#  endif
#endif

typedef struct _TarHeader {
  char name[100];
  uint64_t file_size;
  time_t last_modified;
  
  uint64_t position;
  struct _TarHeader* next;
} TarHeader;

typedef struct {
  char path[512];
  TarHeader* header;
} Tar;

// Parses a tar file
CENTAR_API CENTAR_BOOL ctar_parse(const char* path, Tar* out);

// Frees a tar file
CENTAR_API void ctar_free(Tar* tar);

// Finds a header of a desired file
// Returns NULL if none was found
CENTAR_API TarHeader* ctar_find(Tar* tar, const char* name);

// Allocates a block of memory and writes the file from the tar archive into it
CENTAR_API char* ctar_read(Tar* tar, const char* name,
                           long int* file_size, CENTAR_BOOL null_terminated);

// Writes the file from the tar archive into a block of memory
CENTAR_API CENTAR_BOOL ctar_read_into(Tar* tar, const char* name, char* memory,
                                      long int* file_size, CENTAR_BOOL null_terminated);

// Renames a given file from the tar archive
CENTAR_API void ctar_rename(Tar* tar, const char* name, const char* new_name);

// Writes an already existing tar archive
CENTAR_API void ctar_export(Tar* tar, const char* path);

// Prepare for writing files
CENTAR_API FILE* ctar_begin_write(const char* path);

// Write a file
CENTAR_API CENTAR_BOOL ctar_write(FILE* file, const char* name, char* memory, long int memory_size); 

// Stop writing
CENTAR_API void ctar_end_write(FILE* file);

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

CENTAR_API TarHeader ctar_parse_raw(TarHeaderRaw* raw) {
  TarHeader header = {0};
  strcpy((char*)header.name, (char*)raw->name);
  header.file_size = strtol((const char*)raw->file_size, NULL, 8);
  header.last_modified = strtol((const char*)raw->last_modified, NULL, 8);
  
  return header;
};

CENTAR_API CENTAR_BOOL ctar_parse(const char* path, Tar* out) {
  FILE* file = fopen(path, "rb");
  
  if (!file) {
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

CENTAR_API void ctar_free(Tar* tar) {
  if (!tar) return;
  
  // Free all headers
  TarHeader* head = tar->header;
  TarHeader* tmp;
  while (head) {
    tmp = head;
    head = (TarHeader*)head->next;
    CENTAR_FREE(tmp);
  }
}

CENTAR_API TarHeader* ctar_find(Tar* tar, const char* name) {
  if (!tar || !name) return NULL;
  
  for (TarHeader* t = tar->header; t != NULL; t = t->next) {
    if (strcmp(t->name, name) == 0) {
      return t;
    }
  }
  
  return NULL;
}

CENTAR_API char* ctar_read(Tar* tar, const char* name,
                           long int* file_size, CENTAR_BOOL null_terminated) {
  if (!tar || !name) return NULL;
  
  TarHeader* t = ctar_find(tar, name);
  
  if (!t) return NULL;
  
  uint64_t memory_size = t->file_size + ((null_terminated != 0) ? 1 : 0);
  
  char* memory = (char*)CENTAR_MALLOC(memory_size);
  
  // Read into created memory block
  if (ctar_read_into(tar, name, memory, file_size, null_terminated) == 0) {
    return NULL;
  }
  
  return memory;
}

CENTAR_API CENTAR_BOOL ctar_read_into(Tar* tar, const char* name, char* memory,
                                      long int* file_size, CENTAR_BOOL null_terminated) {
  if (!tar || !name || !memory) return 0;
  
  TarHeader* t = ctar_find(tar, name);
  
  if (!t) return 0;
  
  if (file_size) {
    *file_size = t->file_size + ((null_terminated != 0) ? 1 : 0);
  }
  
  FILE* file = fopen(tar->path, "rb");
  
  if (!file) return 0;
  
  fseek(file, t->position, SEEK_SET);
  fread(memory, t->file_size, 1, file);
  
  fclose(file);
  
  if (null_terminated != 0) {
    memory[t->file_size] = '\0';
  }
  
  return 1;
}

CENTAR_API void ctar_rename(Tar* tar, const char* name, const char* new_name) {
  if (!tar || !name || !new_name) return;
  
  TarHeader* t = ctar_find(tar, name);
  
  if (!t) return;
  
  strncpy(t->name, new_name, 100);
}

CENTAR_API void ctar_export(Tar* tar, const char* path) {
  if (!tar || !path) return;
  
  FILE* out = ctar_begin_write(path);
  
  for (TarHeader* t = tar->header; t != NULL; t = t->next) {
    char* tmp = ctar_read(tar, t->name, NULL, 0);
    
    ctar_write(out, t->name, tmp, t->file_size);
    free(tmp);
  }
  
  ctar_end_write(out);
}

CENTAR_API FILE* ctar_begin_write(const char* path) {
  if (!path) return NULL;
  
  FILE* file = fopen(path, "wb");
  
  return file;
}

CENTAR_API CENTAR_BOOL ctar_write(FILE* file, const char* name, char* memory, long int memory_size) {
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

CENTAR_API void ctar_end_write(FILE* file) {
  if (!file) return;
  
  // Write null header
  fwrite(CENTAR_NULL, 512, 1, file);
  fclose(file);
}

#endif // CENTAR_IMPLEMENTATION

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - zlib License

(C) Copyright 2018-2020 Tom Mol

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
