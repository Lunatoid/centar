# Centar
Simple public domain .tar reader/writer.

Centar is a really simple to use single-file library for reading and writing .tar files.

# Library
## Reading
```cpp
Tar tar;
CENTAR_BOOL success = ctar_parse("path/to/file.tar", &tar);
```

It will return 0 on failure and 1 on success.
`ctar_parse()` will parse all the file headers in the file and store
them in a linked list.


You can enumerate over all the headers like so:

```cpp
for (TarHeader* header = tar.header; header != NULL; header = header->next) {
   ...
}
```

After you're done using the tar archive, you can free it with `ctar_free(&tar)`
To read data you can either use `ctar_read()`, which will allocate a block of memory
using `CENTAR_MALLOC` and reads the data into it.


You can also use ctar_read_into() to read the data into a block of memory, make sure
the block of memory is large enough to store the data.


To find a header, you can use `ctar_find()`, and to rename a file you can use `ctar_rename()`.

## Writing
 To write a tar file you can do this:
 ```cpp
FILE* out = ctar_begin_write("path/to/file.tar");

const char* some_data = "This is some text file that we're gonna write.";
ctar_write(out, "hello.txt", some_data, strlen(some_data));
ctar_end_write(out);
```

If you want to export an existing `Tar` variable, you can use `ctar_export(&tar, "path/to/file.tar");`.

One thing to note is that you shouldn't export a tar file to the tar file that you opened.
`ctar_export()` will read the original archive and then read each file into memory and write it to the new file.

# Command-Line Tool
In the example programs there is a simple command-line tool.

Like the library it should be simple to use. ([unlike the Linux `tar` command](https://www.xkcd.com/1168/))

| Option           | Description                | Arguments      |
| ---------------- | -------------------------- | -------------- |
| `-h` `--help`    | Shows the help             | -              |
| `-c` `--create`  | Creates a new archive      | files...       |
| `-a` `--add`     | Adds a file to the archive | files...       |
| `-r` `--rename`  | Renames a file             | file, new name |
| `-d` `--delete`  | Deletes a file             | files...       |
| `-l` `--list`    | Lists all files            | -              |
| `-e` `--extract` | Extracts files             | files...       |

