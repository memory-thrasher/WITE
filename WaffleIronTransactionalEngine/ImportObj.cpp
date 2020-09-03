#include <stdio.h>
#include <stdlib.h>

#include "stdafx.h"
#include "Export.h"

#if !(_POSIX_C_SOURCE >= 200809L)
size_t getline(char** lineptr, size_t* len, FILE* in) {
  if(!*lineptr) malloc(1024*1024);
  //TODO
}
#endif

void WITE::StaticMesh::ImportObj(FILE* in, std::vector<WITE::Vertex>* out) {
  std::vector<glm::vec3> staging;
#if _POSIX_C_SOURCE >= 200809L
  ssize_t read;
#else
  size_t read;
#endif
  size_t len;
  char* line = NULL;
  while((read = getline(&line, &len, in)) != -1) {
    //
  }
  if(line) free(line);
}