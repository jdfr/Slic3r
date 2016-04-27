#include "TriangleMesh.hpp"
#include <stdio.h>
#include <ctype.h>

int main(int argc, char **argv) {

  if (argc<3) {
    fprintf(stderr, "USAGE: %s INPUT.STL OUTPUT.STL [(NEGATIVE | 000 | SHIFT x y z)]\n\nThis utility reads a 3D mesh from a STL file, translates it, ans writes the result to an output stl file. If no additional arguments are given, it centers the object in the XY plane and displaces it so the lowest coordinate is at 0. If an additional argument starting with the letter 'n' ('negative') is given, the Z displacement is so the highest coordinate is 0. If an additional argument starting with '0' ('000') is given, it moves the object such that the minimal coordinate is 0 in all axes. If an additional argument starting with 's' ('shift') is given, it takes another three arguments which represent the shift on each axis.\n", argv[0]);
    return -1;
  }
  char *filename=argv[1];
  
  bool negative = (argc>=4) && (tolower(argv[3][0])=='n');
  
  bool shift    = (argc>=4) && (tolower(argv[3][0])=='s');
  
  bool toorigin = (argc>=4) && (        argv[3][0] =='0');
  
  if (shift && (argc<7)) {
    fprintf(stderr, "If SHIFT option is used, you must also provide x, y and z shifts.\n");
    return -1;
  }
  
  Slic3r::TriangleMesh *mesh = new Slic3r::TriangleMesh();
  mesh->stl.err = stderr;

  mesh->ReadSTLFile(filename);
  
  if (mesh->needed_repair()) {
    mesh->repair();
  }

  double shiftX, shiftY, shiftZ;
  
  if (shift) {
    char * endptr;
    shiftX = strtod(argv[4], &endptr); if ((*endptr) != 0) { fprintf(stderr, "error parsing X shift: <%s>\n", argv[4]); return -1; }
    shiftY = strtod(argv[5], &endptr); if ((*endptr) != 0) { fprintf(stderr, "error parsing Y shift: <%s>\n", argv[5]); return -1; }
    shiftZ = strtod(argv[6], &endptr); if ((*endptr) != 0) { fprintf(stderr, "error parsing Z shift: <%s>\n", argv[6]); return -1; }
  } else if (toorigin) {
    Slic3r::BoundingBoxf3 bb = mesh->bounding_box();
    
    shiftX = -bb.min.x;
    shiftY = -bb.min.y;
    shiftZ = -bb.min.z;
  } else {
    Slic3r::BoundingBoxf3 bb = mesh->bounding_box();
    
    double centerX = (bb.min.x + bb.max.x)/2;
    double centerY = (bb.min.y + bb.max.y)/2;
    
    shiftX = -centerX;
    shiftY = -centerY;
    shiftZ = negative ? -bb.max.z : -bb.min.z;
  }
  
  
  mesh->translate(shiftX, shiftY, shiftZ);
  
  mesh->write_binary(argv[2]);
  
  delete mesh; 
  return 0;
}
