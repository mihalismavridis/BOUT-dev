/****************************************************************
 * DALF3 model
 * 
 * Four-field model for electron pressure, vorticity, A|| and
 * parallel velocity
 *
 * References:
 *
 *   B.Scott, Plasma Phys. Contr. Fusion 39 (1997) 1635
 *
 *   B.Scott, "Drift Wave versus Interchange Turbulence in
 *             Tokamak Geometry: Linear versus Nonlinear
 *             Mode Structure"
 *             arXiv:physics/0207126  Feb 2001
 *
 * NOTE: The normalisation used here is different to in the above
 *       papers. See manual in doc/ subdirectory for details
 *
 ****************************************************************/

#include <bout.hxx>
#include <boutmain.hxx>

#include <utils.hxx>
#include <invert_laplace.hxx>
#include <math.h>

// Constants
const BoutReal MU0 = 4.0e-7*PI;
const BoutReal Charge = 1.60217646e-19; // electron charge e (C)
const BoutReal Mi = 2.0*1.67262158e-27; // Ion mass
const BoutReal Me = 9.1093816e-31;  // Electron mass
const BoutReal Me_Mi = Me / Mi; // Electron mass / Ion mass

// Poisson brackets: b0 x Grad(f) dot Grad(g) / B = [f, g]
// Method to use: BRACKET_ARAKAWA, BRACKET_STD or BRACKET_SIMPLE
const BRACKET_METHOD bm = BRACKET_STD;

// Evolving quantities
Field3D Vort, Ajpar, Pe, Vpar;

Field3D phi, apar, jpar;

Field2D B0, Pe0, Jpar0;
Vector2D b0xcv;

Field2D eta; // Collisional damping (resistivity)
BoutReal beta_hat, mu_hat;
BoutReal mu_par;

int phi_flags, apar_flags;
bool ZeroElMass, estatic; 
bool curv_kappa;
bool flat_resist;
BoutReal mul_resist;

FieldGroup comms;

int physics_init(bool restarting) {
  
  /////////////////////////////////////////////////////
  // Load data from the grid
  
  GRID_LOAD(Jpar0);
  
  Field2D Ni0, Te0;
  GRID_LOAD2(Ni0, Te0);
  Ni0 *= 1e20; // To m^-3
  Pe0 = Charge * Ni0 * Te0; // Electron pressure in Pascals
  SAVE_ONCE(Pe0);
  
  // Load curvature term
  b0xcv.covariant = false; // Read contravariant components
  mesh->get(b0xcv, "bxcv"); // mixed units x: T y: m^-2 z: m^-2

  // Metric coefficients
  Field2D Rxy, Bpxy, Btxy, hthe;
  Field2D I; // Shear factor
  
  if(mesh->get(Rxy,  "Rxy")) { // m
    output.write("Error: Cannot read Rxy from grid\n");
    return 1;
  }
  if(mesh->get(Bpxy, "Bpxy")) { // T
    output.write("Error: Cannot read Bpxy from grid\n");
    return 1;
  }
  mesh->get(Btxy, "Btxy"); // T
  mesh->get(B0,   "Bxy");  // T
  mesh->get(hthe, "hthe"); // m
  mesh->get(I,    "sinty");// m^-2 T^-1
  
  //////////////////////////////////////////////////////////////
  // Options

  Options *globalOptions = Options::getRoot();
  Options *options = globalOptions->getSection("dalf3");
  
  OPTION(options, phi_flags, 0);
  OPTION(options, apar_flags, 0);
  OPTION(options, estatic, false);
  OPTION(options, ZeroElMass, false);
  OPTION(options, curv_kappa, false);
  OPTION(options, flat_resist, false);
  OPTION(options, mul_resist, 1.0);
  
  // SHIFTED RADIAL COORDINATES

  if(mesh->ShiftXderivs) {
    if(mesh->IncIntShear) {
      // BOUT-06 style, using d/dx = d/dpsi + I * d/dz
      mesh->IntShiftTorsion = I;
      
    }else {
      // Dimits style, using local coordinate system
      b0xcv.z += I*b0xcv.x;
      I = 0.0;  // I disappears from metric
    }
  }
  
  ///////////////////////////////////////////////////
  // Normalisation
  
  BoutReal Tenorm = max(Te0, true);
  BoutReal Nenorm = max(Ni0, true);
  BoutReal Bnorm = max(B0, true);

  // Sound speed in m/s
  BoutReal Cs = sqrt(Charge*Tenorm / Mi);
  
  // drift scale
  BoutReal rho_s = Cs * Mi / (Charge * Bnorm);
  
  beta_hat = 4.e-7*PI * Charge*Tenorm * Nenorm / (Bnorm*Bnorm);
  
  if(ZeroElMass) {
    mu_hat = 0.;
  }else
    mu_hat = Me / Mi;
  
  // Spitzer resistivity 
  if(flat_resist) {
    // eta in Ohm-m. NOTE: ln(Lambda) = 20
    eta = 0.51*1.03e-4*20.*pow(Tenorm, -1.5);
  }else {
    eta = 0.51*1.03e-4*20.*(Te0^(-1.5)); 
  }

  // Plasma quantities
  Jpar0 /= Nenorm*Charge*Cs;
  Pe0 /= Nenorm*Charge*Tenorm;

  // Coefficients
  eta *= Charge * Nenorm / Bnorm;

  b0xcv.x /= Bnorm;
  b0xcv.y *= rho_s*rho_s;
  b0xcv.z *= rho_s*rho_s;

  // Metrics
  Rxy /= rho_s;
  hthe /= rho_s;
  I *= rho_s*rho_s*Bnorm;
  Bpxy /= Bnorm;
  Btxy /= Bnorm;
  B0 /= Bnorm;

  mesh->dx /= rho_s*rho_s*Bnorm;
  
  ///////////////////////////////////////////////////
  // CALCULATE METRICS
  
  mesh->g11 = (Rxy*Bpxy)^2;
  mesh->g22 = 1.0 / (hthe^2);
  mesh->g33 = (I^2)*mesh->g11 + (B0^2)/mesh->g11;
  mesh->g12 = 0.0;
  mesh->g13 = -I*mesh->g11;
  mesh->g23 = -Btxy/(hthe*Bpxy*Rxy);
  
  mesh->J = hthe / Bpxy;
  mesh->Bxy = B0;
  
  mesh->g_11 = 1.0/mesh->g11 + ((I*Rxy)^2);
  mesh->g_22 = (B0*hthe/Bpxy)^2;
  mesh->g_33 = Rxy*Rxy;
  mesh->g_12 = Btxy*hthe*I*Rxy/Bpxy;
  mesh->g_13 = I*Rxy*Rxy;
  mesh->g_23 = Btxy*hthe*Rxy/Bpxy;
  
  mesh->geometry(); // Calculate quantities from metric tensor

  SOLVE_FOR3(Vort, Pe, Vpar);
  comms.add(Vort, Pe, Vpar);
  if(!(estatic && ZeroElMass)) {
    SOLVE_FOR(Ajpar);
    comms.add(Ajpar);
  }
  comms.add(phi);

  phi.setBoundary("phi");
  apar.setBoundary("apar");
  jpar.setBoundary("jpar");
}

// Curvature operator
const Field3D Kappa(const Field3D &f) {
  if(curv_kappa) {
    // Use the b0xcv vector from grid file
    return -2.*b0xcv*Grad(f) / B0;
  }
  
  return -2.*bracket(log(B0), f, bm);
}

const Field3D Grad_parP_LtoC(const Field3D &f) {
  return Grad_par_LtoC(f) - beta_hat * bracket(apar, f);
}

const Field3D Grad_parP_CtoL(const Field3D &f) {
  return Grad_par_CtoL(f) - beta_hat * bracket(apar, f);
}

int physics_run(BoutReal time) {

  // Invert vorticity to get electrostatic potential
  phi = invert_laplace(Vort*B0, phi_flags);
  phi.applyBoundary();
  
  // Communicate evolving variables and phi
  mesh->communicate(comms);

  // Calculate apar and jpar
  if(estatic) {
    // Electrostatic
    apar = 0.;
    if(ZeroElMass) {
      // Not evolving Ajpar
      jpar = Grad_par_CtoL(Pe - phi) / eta;
      jpar.applyBoundary();
    }else {
      jpar = Ajpar / mu_hat;
    }
  }else {
    // Electromagnetic
    if(ZeroElMass) {
      // Ignore electron inertia term
      apar = Ajpar / beta_hat;
    }else {
      // All terms - solve Helmholtz equation
      Field2D a = beta_hat;
      Field2D d = -mu_hat;
      apar = invert_laplace(Ajpar, apar_flags, &a, NULL, &d);
      apar.applyBoundary();
    }
    jpar = -Delp2(apar);
    jpar.applyBoundary();
  }
  
  mesh->communicate(apar, jpar);

  // Vorticity equation
  ddt(Vort) = 
    - bracket(phi, Vort, bm)    // ExB advection
    + B0*B0*Grad_parP_LtoC(jpar/B0)
    - B0*Kappa(Pe)
    ;
  
  // Parallel Ohm's law
  if(!(estatic && ZeroElMass)) {
    // beta_hat*apar + mu_hat*jpar
    ddt(Ajpar) =
      - mu_hat*bracket(phi, jpar, bm)
      + Grad_parP_CtoL(Pe0 + Pe - phi)
      - eta*jpar
      ;
  }
  
  // Parallel velocity
  ddt(Vpar) = 
    - bracket(phi, Vpar, bm)
    - Grad_parP_CtoL(Pe + Pe0) 
    + mu_par * Grad2_par2(Vpar)
    ;

  // Electron pressure
  ddt(Pe) =
    - bracket(phi, Pe + Pe0, bm)
    + (Pe0 + Pe) * (
                    Kappa(phi - Pe)
                    + B0*Grad_parP_LtoC( (jpar - Vpar)/B0 )
                    )
    ;
  
  return 0;
}
