#include "TriangleMesh.hpp"
#include <stdio.h>
#include <ctype.h>

int main(int argc, char **argv) {

  if (argc<3) {
    fprintf(stderr, "USAGE: %s INPUT.STL OUTPUT.STL [NEGATIVE]\n  This utility reads a 3D mesh from a STL file, centers it in the XY plane and displaces it so the lowest coordinate is at 0. If an additional argument starting with the letter 'n' ('negative') is given, the Z displacement is so the highest coordinate is 0.\n", argv[0]);
    return -1;
  }
  char *filename=argv[1];
  
  bool negative = (argc>=4) && (tolower(argv[3][0])=='n');
  
  Slic3r::TriangleMesh *mesh = new Slic3r::TriangleMesh();
  mesh->stl.err = stderr;

  mesh->ReadSTLFile(filename);
  
  if (mesh->needed_repair()) {
    mesh->repair();
  }

  Slic3r::BoundingBoxf3 bb = mesh->bounding_box();
  
  double centerX = (bb.min.x + bb.max.x)/2;
  double centerY = (bb.min.y + bb.max.y)/2;
  
  mesh->translate(-centerX, -centerY, negative ? -bb.max.z : -bb.min.z);
  
  mesh->write_binary(argv[2]);
  
  delete mesh; 
  return 0;
}
