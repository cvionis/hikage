function String 
OS_ReadFile(Arena *arena, String path)
{
  U8 *data     = 0;
  U32 filesize = 0;

  FILE *file = fopen((char *)path.data, "rb");
  if (file) {
    fseek(file, 0, SEEK_END);
    filesize = ftell(file);
    rewind(file);
    data = ArenaPushArray(arena, U8, filesize + 1);
    fread(data, 1, filesize, file);
    data[filesize] = '\0';
    fclose(file);
  }

  String str = {
    .data  = data,
    .count = filesize,
  };
  return str;
}
