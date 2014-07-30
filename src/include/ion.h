#ifndef CCMD_ion_h
#define CCMD_ion_h

#include "ion_trap.h"
#include "stochastic_heat.h"
#include "stats.h"
#include "ccmd_sim.h"

#include <memory>

class Vector3D;
class IonHistogram;
template <class T> class Stats;

class Ion_trap;
typedef std::shared_ptr<Ion_trap> Ion_trap_ptr;

class Ion {
public:
    Ion(const Ion_type& type);
    virtual ~Ion() {}
    // shifts ion position
    void move(const Vector3D& move_va) { pos += move_va; }
    
    // free flight 
    void drift(const double dt);

    // velocity modifying functions
    // Subclasses must provide their own force calculation
    virtual void kick(double dt) = 0;
    virtual void kick(double dt, const Vector3D& f);
    virtual void velocity_scale(double dt) {}
    virtual void heat(double dt) {}

    // These should only be called once on initialising the ion;
    void set_position(const Vector3D& r) { pos = r; }
    void set_velocity(const Vector3D& v) { vel = v; }

    void recordKE(IonHistogram& ionHistogram) const;
    
    // accessor functions
    const Ion_type& get_type() const {return ion_type; }
    std::string name() const {return ion_type.name;}
    std::string formula() const {return ion_type.formula;}
    const Vector3D& get_pos() const {return pos;}
    const Vector3D& get_vel() const {return vel;}
    double get_mass() const {return mass;}
    double get_charge() const {return charge;}
    const Stats<Vector3D> get_posStats() const {return posStats;}
    const Stats<Vector3D> get_velStats() const {return velStats;}
    
    void updateStats();

    void update_from(const Ion_type& from);

    Ion(const Ion&) = delete;
    const Ion& operator=(const Ion&) = delete;
protected:
    const Ion_type& ion_type;
    Vector3D pos;
    Vector3D vel;
    double mass;
    int charge;
    
    
    // Ion statistics
    Stats<Vector3D> posStats;
    Stats<Vector3D> velStats;
    
};


class Trapped_ion : public Ion {
public:
    Trapped_ion(const Ion_trap_ptr ion_trap,const Ion_type& type);
    ~Trapped_ion() {}

    virtual void kick(double dt);
    virtual void velocity_scale(double dt) {}

    Trapped_ion(const Trapped_ion&) = delete;
    const Trapped_ion& operator=(const Trapped_ion&) = delete;
private:
    const Ion_trap_ptr trap;
};

class Lasercooled_ion : public Trapped_ion {
public:
    Lasercooled_ion(const Ion_trap_ptr ion_trap,const Ion_type& type, const Sim_params& sp);
    ~Lasercooled_ion() {}
    
    void kick(double dt);
    void velocity_scale(double dt);
    void heat(double dt);

    Lasercooled_ion(const Lasercooled_ion&) = delete;
    const Lasercooled_ion& operator=(const Lasercooled_ion&) = delete;
private:
    double beta;
    
    // photon recoil heating
    Stochastic_heat heater;

    Vector3D get_friction() const;
};

/// Ion pointer type.
typedef std::shared_ptr<Ion> Ion_ptr;

#endif

