#include <stdio.h>
#include <string.h>
int main() {
  FILE* fp;
  fp = fopen("mnt/data11.txt", "w");

  int ret = fprintf(fp, "Hello");
  printf("fprintf ret: %i\n", ret);

  fclose(fp);

  fp = fopen("mnt/data11.txt", "r");
  printf("fp: %p\n", (void*)fp);
  char buffer[5] = {0};

  int bytesRead = fread(buffer, 1, sizeof(buffer), fp);

  const char* content = "Hello";

  if (bytesRead != strlen(content) || strcmp(content, buffer) != 0) {
    fclose(fp);
    printf("Bytes read was %d but actual bytes is %ld\n", bytesRead, strlen(content));
    printf("Wrong content! File contains %s\n", buffer);
    return 1;
  }
  fclose(fp);

  printf("Passed all tests!\n");
  return 0;
}