#include "TriangleMesh.hpp"
#include <stdio.h>

int main(int argc, char **argv) {

  if (argc<2) {
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
  
  printf("bounding box:\n");
  printf("X: %f %f\n", bb.min.x, bb.max.x);
  printf("Y: %f %f\n", bb.min.y, bb.max.y);
  printf("Z: %f %f\n", bb.min.z, bb.max.z);
  
  delete mesh; 
  return 0;
}
