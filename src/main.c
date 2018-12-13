
#define CENTAR_IMPLEMENTATION
#include "centar.h"

#define IS_OPTION(opt, short_opt, long_opt) (strcmp((opt), (short_opt)) == 0 || strcmp((opt), (long_opt)) == 0)

int main(int argc, char** argv) {
  
  CTAR_BOOL show_help = argc < 3;
  if (argc >= 3) {
    char* tar_file = argv[1];
    
    char* tmp_file = (char*)malloc(strlen(tar_file) + 5);
    strcpy(tmp_file, tar_file);
    strcat(tmp_file, "~tmp");
    
    // Parse options
    char* option = argv[2];
    
    if (IS_OPTION(option, "-h", "--help")) {
      show_help = 1;
    } else if (IS_OPTION(option, "-c", "--create") || IS_OPTION(option, "-a", "--add")) {
      if (argc < 3) {
        printf("\nNot enough arguments for operation 'add'\n");
        return 1;
      }
      
      FILE* out = ctar_begin_write(tmp_file);
      
      if (IS_OPTION(option, "-a", "--add")) {
        // Write existing files
        Tar tar;
        if (ctar_parse(tar_file, &tar) == 0) {
          printf("\nSomething went wrong opening '%s'\n", tar_file);
          return 1;
        }
        
        for (TarHeader* t = tar.header; t != NULL; t = t->next) {
          char* memory = ctar_read(&tar, t->name, NULL, 0);
          ctar_write(out, t->name, memory, t->file_size);
          free(memory);
        }
        
        ctar_free(&tar);
      }
      
      // Write new files
      for (int i = 3; i < argc; ++i) {
        FILE* in = fopen(argv[i], "rb");
        
        if (!in) {
          printf("  * couldn't open file '%s'\n", argv[i]);
          continue;
        }
        
        fseek(in, 0, SEEK_END);
        long int file_size = ftell(in);
        fseek(in, 0, SEEK_SET);
        
        char* memory = (char*)malloc(file_size);
        fread(memory, file_size, 1, in);
        fclose(in);
        
        ctar_write(out, argv[i], memory, file_size);
        free(memory);
      }
      ctar_end_write(out);
      
      // Remove original file and rename temporary file
      if (IS_OPTION(option, "-a", "--add")) {
        remove(tar_file);
      }
      
      rename(tmp_file, tar_file);
      
    } else if (IS_OPTION(option, "-r", "--rename")) {
      if (argc != 5) {
        printf("\nIncorrect argument count for operation 'rename'\n");
        return 1;
      }
      
      Tar tar;
      if (ctar_parse(tar_file, &tar) == 0) {
        printf("\nSomething went wrong opening '%s'\n", tar_file);
        return 1;
      }
      
      ctar_rename(&tar, argv[3], argv[4]);
      ctar_export(&tar, tmp_file);
      
      // Remove original file and rename temporary file
      remove(tar_file);
      rename(tmp_file, tar_file);
      
      ctar_free(&tar);
    } else if (IS_OPTION(option, "-d", "--delete")) {
      if (argc < 3) {
        printf("\nNot enough arguments for operation 'delete'\n");
        return 1;
      }
      
      // Write existing files
      Tar tar;
      if (ctar_parse(tar_file, &tar) == 0) {
        printf("\nSomething went wrong opening '%s'\n", tar_file);
        return 1;
      }
      
      FILE* out = ctar_begin_write(tmp_file);
      
      for (TarHeader* t = tar.header; t != NULL; t = t->next) {
        CTAR_BOOL should_add = 1;
        
        // Check if this needs to be deleted
        for (int i = 3; i < argc; ++i) {
          if (strcmp(t->name, argv[i]) == 0) {
            should_add = 0;
          }
        }
        
        if (should_add != 0) {
          char* memory = ctar_read(&tar, t->name, NULL, 0);
          ctar_write(out, t->name, memory, t->file_size);
          free(memory);
        }
      }
      
      ctar_free(&tar);
      ctar_end_write(out);
      
      // Remove original file and rename temporary file
      remove(tar_file);
      rename(tmp_file, tar_file);
      
    } else if (IS_OPTION(option, "-l", "--list")) {
      Tar tar;
      if (ctar_parse(tar_file, &tar) == 0) {
        printf("\nSomething went wrong opening '%s'\n", tar_file);
        return 1;
      }
      
      printf("\n%s\n", tar_file);
      for (TarHeader* t = tar.header; t != NULL; t = t->next) {
        char* time_str = ctime(&t->last_modified);
        
        *strchr(time_str, '\n') = '\0';
        
        printf("  * %s (%s, %lld bytes)\n", t->name, time_str, t->file_size);
      }
      
      ctar_free(&tar);
    } else if (IS_OPTION(option, "-e", "--extract")) {
      Tar tar;
      if (ctar_parse(tar_file, &tar) == 0) {
        printf("\nSomething went wrong opening '%s'\n", tar_file);
        return 1;
      }
      
      for (int i = 3; i < argc; ++i) {
        long int file_size;
        char* memory = ctar_read(&tar, argv[i], &file_size, 0);
        
        if (!memory) {
          printf("  * couldn't read file '%s'\n", argv[i]);
          continue;
        }
        
        FILE* out = fopen(argv[i], "wb");
        
        if (!out) {
          free(memory);
          printf("  * something went wrong extracting file '%s'\n", argv[i]);
          continue;
        }
        
        fwrite(memory, file_size, 1, out);
        fclose(out);
        
        printf("  * extracting '%s'\n", argv[i]);
        
        free(memory);
      }
      
      ctar_free(&tar);
    } else {
      printf("\nUnknown option '%s'\n", argv[2]);
    }
  }
  
  if (show_help != 0) {
    printf("\nUsage: %s tar_file [option] [arguments]\n", argv[0]);
    printf("  -h, --help    | Shows the help (no arguments)\n");
    printf("  -c, --create  | Creates a new archive (files...)\n");
    printf("  -a, --add     | Adds a file to the archive (files...)\n");
    printf("  -r, --rename  | Renames a file (file, new_name)\n");
    printf("  -d, --delete  | Deletes a file (files...)\n");
    printf("  -l  --list    | Lists all files (no arguments)\n");
    printf("  -e  --extract | Extracts files (files...)\n");
  }
  
  return 0;
}
