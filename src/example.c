
#define CENTAR_IMPLEMENTATION
#include "centar.h"

int main() {
  // Writing files
  printf("Writing export.tar...\n");
  FILE* file = ctar_begin_write("export.tar");
  
  char* str = "Hello, world!";
  ctar_write(file, "file_1.txt", str, strlen(str));
  printf("  > added file_1.txt\n");
  
  char* str2 = "Hello, universe!";
  ctar_write(file, "file_2.txt", str2, strlen(str2));
  printf("  > added file_2.txt\n");
  
  ctar_end_write(file);
  
  // Reading files
  Tar tar;
  ctar_parse("export.tar", &tar);
  
  printf("\nExtracting %s...\n", tar.path);
  for (TarHeader* t = tar.header; t != NULL; t = t->next) {
    printf("  > %s\n", t->name);
    
    FILE* out = fopen(t->name, "wb");
    char* data = ctar_read(&tar, t->name, NULL, 0);
    fwrite(data, t->file_size, 1, file);
    fclose(file);
    
    free(data);
  }
  
  ctar_rename(&tar, "file_1.txt", "renamed.txt");
  ctar_export(&tar, "export2.tar");
  
  ctar_free(&tar);
  
  return 0;
}
