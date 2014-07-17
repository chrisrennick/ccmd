//
//  coulomb_force.h
//


#ifndef CCMD_Coulomb_force_h
#define CCMD_Coulomb_force_h

#include "vector3D.h"
#include "ion_cloud.h"
#include <memory>
#include <vector>
#include <thread>

class Ion_cloud;

// This class calculates and stores a vector of Coulomb 
// forces for an ion cloud

class Coulomb_force {
public:
    Coulomb_force(const Ion_cloud_ptr &ic);
    const std::vector<Vector3D>& get_force();
    
    // return Coulomb force on a particular ion
    Vector3D get_force(size_t i);
    
    // recalculate forces
    void update();

private:
    const Ion_cloud_ptr ionCloud;
    std::vector<Vector3D> ionsCopy;
    std::vector<Vector3D> force;
    void direct_force();
    void split_force(int n);
    
    int max_thread;
    std::thread m_Thread;
    std::vector<std::thread> threads;
    
    // Prevent copying
    Coulomb_force( const Coulomb_force & other ) = delete;
    Coulomb_force& operator=( const Coulomb_force& ) = delete;
};

#endif
