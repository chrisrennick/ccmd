//
//  ion_cloud.cpp
//

#include "ion_cloud.h"

#include "ccmd_sim.h"
#include "ion.h"
#include "vector3D.h"
#include "ImageCollection.h"
#include "IonHistogram.h"
#include "Stats.h"

#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

// To do:   fix aspect ratio
//#include "eigs.h"

using namespace std;

class Hist3D;
int get_nearest_cube(int n);
std::vector<Vector3D> get_lattice(size_t n);

struct compare_ions_by_mass {
    bool operator()(const Ion* lhs, const Ion* rhs) const 
    {
        return lhs->m() < rhs->m();
    }
};

struct position_ions {
    Ion* operator() (Ion* ion, const Vector3D& r) const
    {
        ion->set_position(r);
        return ion;
    }
};

Ion_cloud::Ion_cloud(const Ion_trap& ion_trap, const Cloud_params& params)
    :  trap(&ion_trap), cloud_params(&params)  
{
    // loop over ion types to initialise ion cloud
    typedef map<Ion_type*,int>::const_iterator map_itr;
    for (map_itr p=cloud_params->ion_numbers.begin(); 
                 p!=cloud_params->ion_numbers.end();
               ++p) {
        Ion_type* type_ptr(p->first);
        int number_of_type(p->second);
        
        // loop over ions numeber for type, construct ions using *trap to ensure 
        // that changes to ion trap parameters are felt by the ions
        try {
            for (int i=0; i<number_of_type; ++i) {
                if (type_ptr->is_laser_cooled) {
                    add_ion(new Lasercooled_ion(*trap, *type_ptr));
                } else {
                    add_ion(new Trapped_ion(*trap, *type_ptr));
                }
            }
        } catch (bad_alloc& ba) {
            ostringstream error_msg;
            error_msg << "bad_alloc in ion cloud constructor: " << ba.what();
            throw runtime_error( error_msg.str() );
        }
        
        // sort ions by mass
        sort(ion_vec.begin(),ion_vec.end(), compare_ions_by_mass() );
        
        // generate initial positions
        vector<Vector3D> lattice = get_lattice( number_of_ions() );
        
        // move ions into position
        transform(ion_vec.begin(), ion_vec.end(),
                  lattice.begin(), ion_vec.begin(),
                  position_ions() );
        
        // move cloud centre to the origin
        Vector3D to_origin = -get_cloud_centre( *this );
        move_centre(to_origin);
        
        runStats = false;
    }
}

void Ion_cloud::update_params()
{
    // update types for ions
    for_each(ion_vec.begin(),
             ion_vec.end(), 
             mem_fun(&Ion::update_ion_type));
    
    // update trap parameters used by ions
   for_each(ion_vec.begin(),
            ion_vec.end(), 
            mem_fun(&Ion::update_trap_force));
}

Ion_cloud::~Ion_cloud()
{
    typedef std::vector<Ion*>::iterator ion_itr;
    for (ion_itr ion=ion_vec.begin(); ion<ion_vec.end(); ++ion) {
        delete *ion;
    }
    ion_vec.clear();
}

int get_nearest_cube(int n)
{
    // returns integer which when cubed is closest to n 
	int min_cube = 1;
	int max_cube = 1;
	int i = 1;
	while (max_cube <=n) {
		min_cube = max_cube;
		++i;
		max_cube = i*i*i;
	}
    return (n-min_cube) < (max_cube-n) ? i-1 : i;

}

Vector3D Ion_cloud::get_cloud_centre(const Ion_cloud& ) const
{
    // 	unweighted geometric centre of ion cloud
	Vector3D centre;
    double n_ions = ion_vec.size();
    
	for (int i=0; i<n_ions; ++i)
		centre += ion_vec[i]->r();
    
	centre /= n_ions;
	return centre;
}

void Ion_cloud::move_centre(const Vector3D& v)
{
	// move the whole ion cloud
	for (int i=0; i<ion_vec.size(); ++i) {
		ion_vec[i]->move( v );
	}
}

std::ostream& operator<<(std::ostream& os, const Ion_cloud& ic)
{
    // outputs ion positions as 
    // --------------------------------
    //  Ion         x       y       z
    // --------------------------------
    //
    static bool first_call = true;
    
    if (first_call) {
        std::string header_space(7,' ');
        std::string header_line(33,'-');
        std::string header = " Ion         x       y       z";
    
        os << header_space << header << std::endl;
        os << header_space << header_line << std::endl;
    
        first_call = false;
    }
    
	os.setf(std::ios::fixed);
	os.precision(6);
	os << setiosflags( std::ios::right );
    
	enum XYZ{x,y,z};
	for (int i=0; i<ic.ion_vec.size(); ++i) {
		os << std::setw(10) << i+1 << '\t';
        
		os << std::setw(7) << ic.ion_vec[i]->r(x) << ' '
            << std::setw(7) << ic.ion_vec[i]->r(y) << ' '
            << std::setw(7) << ic.ion_vec[i]->r(z) << std::endl;
	}
    
	os << std::endl;
    
	return os;
}


void Ion_cloud::drift(double t)
{
    for (int i=0; i<ion_vec.size(); ++i) {
        ion_vec[i]->drift(t);
    }
}

struct Kicker_t {
    Kicker_t(double t) : t(t) {}
    void operator()(Ion* ion_to_kick) {
        ion_to_kick->kick(t);
    }
private:
    double t;
};

void Ion_cloud::kick(double t)
{
    for_each(ion_vec.begin(), ion_vec.end(), Kicker_t(t) );
}

void Ion_cloud::kick(double t, const std::vector<Vector3D>& f)
{
    for (int i=0; i<ion_vec.size(); ++i) {
        ion_vec[i]->kick(t, f[i] );
    }
}

struct Velocity_scaler {
    Velocity_scaler(double dt) : dt(dt) {}
    void operator()(Ion* ion_to_scale) {
        ion_to_scale->velocity_scale(dt);
    }
private:
    double dt;
};

void Ion_cloud::velocity_scale(double dt)
{
    for_each(ion_vec.begin(),ion_vec.end(), Velocity_scaler(dt));
}

void Ion_cloud::heat(double t)
{
    for (int i=0; i<ion_vec.size(); ++i) {
        ion_vec[i]->heat(t);;
    }
}

double Ion_cloud::kinetic_energy() const
{
    double e = 0;
    double m;
    for (int i=0; i<ion_vec.size(); ++i) {
        m = ion_vec[i]->mass;
        e += 0.5*m*( ion_vec[i]->vel.norm_sq() );
    }
    return e;
}

double Ion_cloud::coulomb_energy() const
{
    Vector3D r1,r2;
    int q1,q2;
    double e = 0;
    double r;
    
    for (int i=0; i<ion_vec.size(); ++i) {
        r1 = ion_vec[i]->pos;
        q1 = ion_vec[i]->charge;
        for (int j=i+1; j<ion_vec.size(); ++j) {
            r2 = ion_vec[j]->pos;
            q2 = ion_vec[j]->charge;
            
            if (r1 != r2) {
                r = Vector3D::dist(r1,r2);
            } else {
                std::cout << i << ' ' << r1 << ' ' << j << ' ' << r2 << '\n';
                std::abort();
            }
        e += q1*q2/r;
        }
    }
    return e;
}

void Ion_cloud::updateStats()
{
    typedef std::vector<Ion*>::const_iterator ion_itr;
    for (ion_itr ion=ion_vec.begin(); ion!=ion_vec.end(); ++ion)
    {
        (*ion)->updateStats();
    }
}

void Ion_cloud::saveStats(std::string basePath) const {
    std::string fileEnding = "_stats.dat";
    std::string fileName;
    std::string filerow;
    Stats<Vector3D> energy;
    Stats<Vector3D> pos;
    Vector3D avg_energy, var_energy, avg_pos, var_pos;
    double mon2;
    
    typedef std::map <std::string, std::string> FileText;
    FileText fileContents;
    
    typedef std::vector<Ion*>::const_iterator ion_itr;
    for (ion_itr ion=ion_vec.begin(); ion!=ion_vec.end(); ++ion)
    {
        energy = (*ion)->velStats;
        pos = (*ion)->posStats;
        mon2 = ((*ion)->mass)/2;
        avg_energy = energy.average();
        var_energy = energy.variance();
        avg_pos = pos.average();
        var_pos = pos.variance();
        
        std::ostringstream strs;
        strs << avg_energy[0]*mon2 << "\t" << var_energy[0]*mon2 << "\t";
        strs << avg_energy[1]*mon2 << "\t" << var_energy[1]*mon2 << "\t";
        strs << avg_energy[2]*mon2 << "\t" << var_energy[2]*mon2 << "\t";
        strs << avg_pos[0] << "\t" << var_pos[0] << "\t";
        strs << avg_pos[1] << "\t" << var_pos[1] << "\t";
        strs << avg_pos[2] << "\t" << var_pos[2] << "\n";
        filerow = strs.str();
        fileContents[(*ion)->name()] += filerow;
    }
    
    std::string header="#<KE_x>\tvar(KE_x)\t<KE_y>\tvar(KE_y)\t<KE_z>\tvar(KE_z)\t<pos_x>\tvar(pos_x)\t<pos_y>\tvar(pos_y)\t<pos_z>\tvar(pos_z)\n";
    for (FileText::iterator file=fileContents.begin(); file!=fileContents.end(); ++file)
    {
        fileName = basePath + file->first + fileEnding;
        std::ofstream fileStream(fileName.c_str());
        fileStream << header;
        fileStream << file->second;
        fileStream.close();
    }
    
}

void Ion_cloud::update_energy_histogram(IonHistogram& h) const
{
    typedef std::vector<Ion*>::const_iterator ion_itr;
    for (ion_itr ion=ion_vec.begin(); ion!=ion_vec.end(); ++ion)
    {
        (*ion)->recordKE(h);
    }
}

void Ion_cloud::update_position_histogram(ImageCollection& h) const
{
    Vector3D posn;
    Vector3D rotated_pos;
    typedef std::vector<Ion*>::const_iterator ion_itr;
    for (ion_itr ion=ion_vec.begin(); ion!=ion_vec.end(); ++ion)
    {
        posn = (*ion)->r();
        rotated_pos.x = (posn.x+posn.y)/sqrt(2.0);
        rotated_pos.y = (posn.x-posn.y)/sqrt(2.0);
        rotated_pos.z = posn.z;
        h.addIon((*ion)->name(), rotated_pos);
    }
}

void Ion_cloud::scale(double scale_factor)
{
    for (int i=0; i<ion_vec.size(); ++i) {
        ion_vec[i]->pos = ion_vec[i]->pos*scale_factor;
    }
}

std::vector<Vector3D> get_lattice(size_t n)
{
    int side = ceil( pow(n,1.0/3.0) );
    std::vector<Vector3D> lattice;

	Vector3D grid_pos;
    double scale = 2.0;
    
    Vector3D to_centre = Vector3D(0.5,0.5,0.5)*side*scale;
	// Move ions to cubic lattice sites 
	for (int i=0; i<pow(side,3.0); ++i) {
		grid_pos.x =               i%side;
		grid_pos.y =        (i/side)%side;
		grid_pos.z = (i/(side*side))%side;
        
        grid_pos *= scale;
        grid_pos -= to_centre;
        lattice.push_back(grid_pos);
    }
    std::sort(lattice.begin(), lattice.end());
    lattice.resize(n);
    
    return lattice;
}

double Ion_cloud::aspect_ratio() const
{
    // aspect ratio defined by maxmimum extent of ions
    // in x or y and z directions 
    using namespace std;
    
    double x_max = 0.0;
    double y_max = 0.0;
    double z_max = 0.0;

    for (int i=0; i<ion_vec.size(); ++i) {
        x_max = max(x_max, abs(ion_vec[i]->pos.x) );
        y_max = max(y_max, abs(ion_vec[i]->pos.y) );
        z_max = max(z_max, abs(ion_vec[i]->pos.z) );

    };
    return z_max/max(x_max,y_max); 
    
/*
    // Aspect ratio defined by moment of inertia ellipsoid
    // Requires diagonalisation routine
    // NOT PORTABLE YET
 
    double w[3];
    double x,y,z = 0.0;
    
    // Calculate moment of inertia tensor
    double I[9] = {0,0,0,0,0,0,0,0,0};
    for (int i=0; i<ion_vec.size(); ++i) {
        x = ion_vec[i]->pos.x;
        y = ion_vec[i]->pos.y;
        z = ion_vec[i]->pos.z;
        I[0] += y*y + z*z; 
        I[1] += -x*y; 
        I[2] += -x*z; 
        I[4] += x*x + z*z; 
        I[5] += -y*z; 
        I[8] += x*x + y*y;
    };
    I[3] = I[1];
    I[6] = I[2];
    I[7] = I[5];
    
    get_eigenvalues(I,w,3);
    
    //for (int i=0; i<3; ++i) std::cout << w[i] << std::endl;
    
    return w[2]/w[0];
*/
}

Vector3D Ion_cloud::ion_position(size_t ion_index) const
{
    if (ion_index < ion_vec.size())
        return ion_vec[ion_index]->r();
    else
        throw;
}

Vector3D Ion_cloud::ion_velocity(size_t ion_index) const
{
    if (ion_index < ion_vec.size())
        return ion_vec[ion_index]->v();
    else
        throw;
}

int Ion_cloud::ion_charge(size_t ion_index) const
{
    return ion_vec[ion_index]->q();
}

double Ion_cloud::ion_mass(size_t ion_index) const
{
    return ion_vec[ion_index]->m();
}

std::string Ion_cloud::ion_name(size_t ion_index) const
{
    return ion_vec[ion_index]->name();
}

std::string Ion_cloud::ion_formula(size_t ion_index) const
{
    return ion_vec[ion_index]->formula();
}

std::string Ion_cloud::ion_color(size_t ion_index) const
{
    return ion_vec[ion_index]->color();
}

void Ion_cloud::set_ion_position(size_t ion_index, const Vector3D& r)
{
    // throws runtime_error if invalid ion_index used
    if (ion_index < ion_vec.size()) {
        ion_vec[ion_index]->set_position(r);
        return;
    }
    else {
        ostringstream error_msg;
        error_msg << "Error in Ion_cloud::set_ion_position: invalid ion_index: " 
                  << ion_index;
        
        throw runtime_error( error_msg.str() );
    }
}

void Ion_cloud::set_ion_velocity(size_t ion_index, const Vector3D& v)
{
    // throws runtime_error if invalid ion_index used
    if (ion_index < ion_vec.size()) {
        ion_vec[ion_index]->set_velocity(v);
        return;
    }
    else {
        ostringstream error_msg;
        error_msg << "Error in Ion_cloud::set_ion_velocity: invalid ion_index: "
                  << ion_index;
        throw runtime_error( error_msg.str() );
    }
}

struct isIon_type {
    isIon_type(const Ion_type& type_in) : type_(type_in) {}
    bool operator()(Ion* ion) const {
        return &(ion->get_type()) == &type_;
    }
private:
    const Ion_type& type_; 
};

// changes one ion from one type to another, returns true if successful
bool Ion_cloud::change_ion(const std::string& name_in, const std::string& name_out) {
    try {
        const Ion_type& type_in = cloud_params->get_Ion_type_by_name(name_in);
        const Ion_type& type_out = cloud_params->get_Ion_type_by_name(name_out);
        return this->change_ion(type_in, type_out);
    } catch (std::exception& e) {
        std::cerr << e.what();
        return false;
    }
}

// changes one ion from one type to another, returns true if successful
// throws runtime_error if unable to assign new ion
bool Ion_cloud::change_ion(const Ion_type& type_in, const Ion_type& type_out) {
    
    Ion_ptr_vector::iterator ion_to_change;
    ion_to_change = find_if(ion_vec.begin(), ion_vec.end(), isIon_type(type_in) );
    
    if ( ion_to_change == ion_vec.end() ) {
        return false;
    }
    
    Ion* new_ion = 0;
    try {
        // allocates new ion according to new type
        if (type_out.is_laser_cooled)
            new_ion = new Lasercooled_ion(*trap, type_out);
        else
            new_ion = new Trapped_ion(*trap, type_out);
        
    } catch (bad_alloc& ba) {
        ostringstream error_msg;
        error_msg << "bad_alloc in Ion_cloud::change_ion: " << ba.what();
        throw runtime_error( error_msg.str() );
    }    
    
    // keep original position and velocity
    new_ion->set_position( (*ion_to_change)->r() );
    new_ion->set_velocity( (*ion_to_change)->v() );
    
    // delete old ion and change pointer to new ion
    delete *ion_to_change;
    *ion_to_change = new_ion;
    
    return true;
}





