#ifndef __PAR_BNDRY_OP_H__
#define __PAR_BNDRY_OP_H__

#include "boundary_op.hxx"
#include "bout_types.hxx"
#include "parallel_boundary_region.hxx"
#include "utils.hxx"

//////////////////////////////////////////////////
// Base class

class BoundaryOpPar : public BoundaryOpBase {
public:
  BoundaryOpPar() : bndry(NULL), real_value(0.), value_type(REAL) {}
  BoundaryOpPar(BoundaryRegionPar *region, FieldGenerator* value) :
    bndry(region),
    gen_values(value),
    value_type(GEN) {}
  BoundaryOpPar(BoundaryRegionPar *region, Field3D* value) :
    bndry(region),
    field_values(value),
    value_type(FIELD) {}
  BoundaryOpPar(BoundaryRegionPar *region, BoutReal value) :
    bndry(region),
    real_value(value),
    value_type(REAL) {}
  virtual ~BoundaryOpPar() {}

  // Note: All methods must implement clone, except for modifiers (see below)
  virtual BoundaryOpPar* clone(BoundaryRegionPar *region, const list<string> &args) {return NULL; }

  void apply(Field2D &f)
  {
    throw BoutException("Can't apply parallel boundary conditions to Field2D!");
  }
  void apply(Field2D &f, BoutReal t)
  {
    throw BoutException("Can't apply parallel boundary conditions to Field2D!");
  }
  void apply(Field3D &f) {}
  void apply(Field3D &f, BoutReal t) {}

  // Apply to time derivative
  // Unlikely to be used?
  void apply_ddt(Field3D &f) {};

  BoundaryRegionPar *bndry;

protected:

  /// Possible ways to get boundary values
  FieldGenerator* gen_values;
  Field3D* field_values;
  BoutReal real_value;

  /// Where to take boundary values from - the generator, field or BoutReal
  enum ValueType { GEN, FIELD, REAL };
  const ValueType value_type;

  BoutReal getValue(int x, int y, int z, BoutReal t);

};

//////////////////////////////////////////////////
// Implementations

class BoundaryOpPar_dirichlet : public BoundaryOpPar {
public:
  BoundaryOpPar_dirichlet() :
    BoundaryOpPar(NULL, 0.) {}
  BoundaryOpPar_dirichlet(BoundaryRegionPar *region) :
    BoundaryOpPar(region, 0.) {}
  BoundaryOpPar_dirichlet(BoundaryRegionPar *region, FieldGenerator* value) :
    BoundaryOpPar(region, value) {}
  BoundaryOpPar_dirichlet(BoundaryRegionPar *region, Field3D* value) :
    BoundaryOpPar(region, value) {}
  BoundaryOpPar_dirichlet(BoundaryRegionPar *region, BoutReal value) :
    BoundaryOpPar(region, value) {}
  BoundaryOpPar* clone(BoundaryRegionPar *region, const list<string> &args);

  void apply(Field3D &f) {return apply(f, 0);}
  void apply(Field3D &f, BoutReal t);

};

class BoundaryOpPar_neumann : public BoundaryOpPar {
public:
  BoundaryOpPar_neumann() :
    BoundaryOpPar(NULL, 0.) {}
  BoundaryOpPar_neumann(BoundaryRegionPar *region) :
    BoundaryOpPar(region, 0.) {}
  BoundaryOpPar_neumann(BoundaryRegionPar *region, FieldGenerator* value) :
    BoundaryOpPar(region, value) {}
  BoundaryOpPar_neumann(BoundaryRegionPar *region, Field3D* value) :
    BoundaryOpPar(region, value) {}
  BoundaryOpPar_neumann(BoundaryRegionPar *region, BoutReal value) :
    BoundaryOpPar(region, value) {}
  BoundaryOpPar* clone(BoundaryRegionPar *region, const list<string> &args);

  void apply(Field3D &f) {return apply(f, 0);}
  void apply(Field3D &f, BoutReal t);

};

#endif // __PAR_BNDRY_OP_H__