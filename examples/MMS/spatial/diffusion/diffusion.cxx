#include <bout.hxx>
#include <boutmain.hxx>
#include <initialprofiles.hxx>
#include <derivs.hxx>
#include <math.h>
#include <bout/constants.hxx>

Field3D N;

BoutReal Dx, Dy, Dz;
BoutReal Lx, Ly, Lz;

int physics_init(bool restarting) {
  // Get the options
  Options *meshoptions = Options::getRoot()->getSection("mesh");

  meshoptions->get("Lx",Lx,1.0);
  meshoptions->get("Ly",Ly,1.0);

  /*this assumes equidistant grid*/
  mesh->dx = Lx/(mesh->GlobalNx - 2*mesh->xstart);
  
  mesh->dy = Ly/(mesh->GlobalNy - 2*mesh->ystart);
  
  output.write("SIZES: %d, %d, %e\n", mesh->GlobalNy, (mesh->GlobalNy - 2*mesh->ystart), mesh->dy(0,0));

  SAVE_ONCE2(Lx,Ly);

  Options *cytooptions = Options::getRoot()->getSection("cyto");
  OPTION(cytooptions, Dx, 1.0);
  OPTION(cytooptions, Dy, -1.0);
  OPTION(cytooptions, Dz, -1.0);

  SAVE_ONCE3(Dx, Dy, Dz);

  //set mesh
  mesh->g11 = 1.0;
  mesh->g22 = 1.0;
  mesh->g33 = 1.0;
  mesh->g12 = 0.0;
  mesh->g13 = 0.0;
  mesh->g23 = 0.0;

  mesh->g_11 = 1.0;
  mesh->g_22 = 1.0;
  mesh->g_33 = 1.0;
  mesh->g_12 = 0.0;
  mesh->g_13 = 0.0;
  mesh->g_23 = 0.0;
  mesh->geometry();

  // Tell BOUT++ to solve N
  SOLVE_FOR(N);

  return 0;
}

int physics_run(BoutReal t) {
  mesh->communicate(N); // Communicate guard cells

  ddt(N) = 0.0;
  
  if(Dx > 0.0)
    ddt(N) += Dx * D2DX2(N);
  
  if(Dy > 0.0)
    ddt(N) += Dy * D2DY2(N);
  
  if(Dz > 0.0)
    ddt(N) += Dz * D2DZ2(N);
  
  return 0;
}

