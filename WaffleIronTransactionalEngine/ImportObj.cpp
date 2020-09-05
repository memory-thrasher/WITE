#include <stdio.h>
#include <stdlib.h>

#include "stdafx.h"
#include "Export.h"

#if !(_POSIX_C_SOURCE >= 200809L)
size_t getline(char** lineptr, size_t* len, FILE* in) {
  if(!*lineptr) *lineptr = (char*)malloc(1024*1024);
  int read;
  *len = 0;
  while((read = fgetc(in)) != EOF && read != '\n')
    (*lineptr)[(*len)++] = (char)read;
  (*lineptr)[(*len)] = 0;//null term string out
  return read == EOF ? -1 : 0;
}
#endif

void WITE::StaticMesh::ImportObj(FILE* in, std::vector<WITE::Vertex>* out) {
  std::vector<glm::vec3> verts, norms;//TODO texture coords
#if _POSIX_C_SOURCE >= 200809L
  ssize_t read;
#else
  size_t read;
#endif
  size_t lineIdx = 0;//will be auto removed bu O3 in release, useful for breakpoints
  size_t len;
  char* line = NULL;
  char* sub;
  WITE::Vertex temp;
  while((read = getline(&line, &len, in)) != -1) {
    lineIdx++;
    strcat(line, " ");//tokenizer breaks when string does not end with a delim
    sub = strtok(line, " ");
    if(!strcmp(sub, "v")) {
      verts.push_back(glm::vec3(std::stof(strtok(NULL, " ")), std::stof(strtok(NULL, " ")), std::stof(strtok(NULL, " "))));
    } else if(!strcmp(sub, "vn")) {
      norms.push_back(glm::vec3(std::stof(strtok(NULL, " ")), std::stof(strtok(NULL, " ")), std::stof(strtok(NULL, " "))));
    } else if(!strcmp(sub, "f")) {
      while(sub = strtok(NULL, " ")) {//assumes face strategy is the same as the renderer. Triangles only or else garbage out.
        std::string::size_type i = 0;
        std::string vertex = sub;
        temp.pos = verts[std::stoi(vertex, &i)-1];
        if(sub[i++] == '/') {//has more
          if(sub[i] != '/') {//has texture
            //TODO texture (and don't change i)
          }
          if(sub[i++] == '/') {//has normal
            temp.norm = norms[std::stoi(vertex.substr(i))-1];
          }
        }
        out->push_back(temp);
      }
    }
  }
  if(line) free(line);
}