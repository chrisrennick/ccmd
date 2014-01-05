//
//  ccmd_sim.h
//

// 
// These classes contain parameters which are used to define the 
// molecular dynamics simulation.
//
// Improvments: use XML to store/load parameters
//              simulation parent class
//

#ifndef CCMD_ccmd_sim_h
#define CCMD_ccmd_sim_h

#include <string>
#include <vector>
#include <map>
#include <list>
#include <string>

#include <stdexcept>


class Trap_params {
public:
    Trap_params(const std::string& file_name);

    // Radiofrequency parameters
    
    /// Enumeration of avaliable trap types.
    enum Waveform {cosine, digital, waveform};
    Waveform wave;              /// Type of RF waveform applied to trap
    double freq;                /// Trap RF frequency            (Hz)
    
    // Trap voltage parameters
    double v_rf;                /// Amplitude of RF voltage      (V)
    double v_end;               /// Endcap electrode voltage     (V)
    
    // Trap geometry parameters
    double eta;                 /// Trap geometry parameter
    double r0;                  /// Electrode radius             (m)
    double z0;                  /// Central electrode length     (m)
    
    
    double tau;                 /// Waveform duty cycle.
    std::string waveformFile;   /// File containing waveform data.
};


class Integration_params {
public:
    Integration_params(const std::string& file_name);
    
    double time_step;       /// Time interval, Units are 2/Omega = 1/(pi*f))
    
    // respa_steps == RESPA algorithm inner loop number 
    //
    // respa_steps == 1 is equivalent to a Velocity Verlet 
    // or Leapfrog algorithm
    //
    // See: M. Tuckerman, B. J. Berne and G. J. Martyna
    //      J. Chem. Phys. 92, 1990 (1992)
    //

    /// Number of RESPA inner loop steps between Coulomb force updates
    int respa_steps;
    double cool_steps;     /// Number of timesteps for cooling stage.
    double hist_steps;     /// Number of steps to collect statistics.
};


class Ion_type {
    public:
    int number;             /// Number of these in the trap.
    std::string name;       /// Name to call ion
    std::string formula;    /// Chemical formula
    
    // Ion physical properties
    double mass;            /// Mass of ion in atomic mass units
    int charge;             /// Charge of ion in electron charge units
    double beta;            /// Velocity damping coefficient for laser cooling
    double recoil;          /// Magnitude of stochastic heating
    double direction;       /// Ratio of left-to-right cooling intensity
    
    bool is_laser_cooled;
    bool is_heated;
};


class Cloud_params {
public:
    Cloud_params(const std::string& file_name);
    
    /// List to hold an Ion_type for each ion type used.
    std::list<Ion_type> ionTypeList;
};
        

#endif
