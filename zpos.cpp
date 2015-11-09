#include "TriangleMesh.hpp"
#include <stdio.h>

int main(int argc, char **argv) {

  if (argc<3) {
    fprintf(stderr, "%d IS AN INCORRECT NUMBER OF ARGUMENTS!!!", argc-1);
    return -1;
  }
  char *filename=argv[1];
  
  Slic3r::TriangleMesh *mesh = new Slic3r::TriangleMesh();
  mesh->stl.err = stderr;

  mesh->ReadSTLFile(filename);
  
  if (mesh->needed_repair()) {
    mesh->repair();
  }

  Slic3r::BoundingBoxf3 bb = mesh->bounding_box();
  
  mesh->translate(0, 0, -bb.min.z);
  
  mesh->write_binary(argv[2]);
  
  delete mesh; 
  return 0;
}
