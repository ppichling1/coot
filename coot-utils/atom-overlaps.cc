/* coot-utils/atom-overlaps.cc
 * 
 * Copyright 2015 by Medical Research Council
 * Author: Paul Emsley
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include <stdlib.h> // needed from abs()

#include "atom-overlaps.hh"
#include "coot-coord-utils.hh"
// #include "coot-h-bonds.hh"

coot::atom_overlaps_container_t::atom_overlaps_container_t(mmdb::Residue *res_central_in,
							   const std::vector<mmdb::Residue *> &neighbours_in,
							   mmdb::Manager *mol_in,
							   const protein_geometry *geom_p_in) {
   geom_p = geom_p_in;
   res_central = res_central_in;
   neighbours = neighbours_in;
   mol = mol_in;
   clash_spike_length = 0.5;
   init();

}

// is this used?
coot::atom_overlaps_container_t::atom_overlaps_container_t(mmdb::Residue *res_central_in,
							   mmdb::Residue *neighbour,
							   mmdb::Manager *mol_in,
							   const protein_geometry *geom_p_in) {
   geom_p = geom_p_in;
   res_central = res_central_in;
   neighbours.push_back(neighbour);
   mol = mol_in;
   clash_spike_length = 0.5;
   init();

}

// clash spike_length should be 0.5;
coot::atom_overlaps_container_t::atom_overlaps_container_t(mmdb::Residue *res_central_in,
							   const std::vector<mmdb::Residue *> &neighbours_in,
							   mmdb::Manager *mol_in,
							   const protein_geometry *geom_p_in,
							   double clash_spike_length_in,
							   double probe_radius_in) {
   probe_radius = probe_radius_in;
   geom_p = geom_p_in;
   res_central = res_central_in;
   neighbours = neighbours_in;
   mol = mol_in;
   clash_spike_length = clash_spike_length_in;
   init();

}

coot::atom_overlaps_container_t::atom_overlaps_container_t(mmdb::Manager *mol_in,
							   const protein_geometry *geom_p_in,
							   double clash_spike_length_in,
							   double probe_radius_in) {
   geom_p = geom_p_in;
   res_central = 0;
   mol = mol_in;
   clash_spike_length = 0.5;
   probe_radius = probe_radius_in;
   init_for_all_atom();
}



void
coot::atom_overlaps_container_t::init() {

   overlap_mode = CENTRAL_RESIDUE;

   have_dictionary = false; // initially.

   if (res_central) {

      std::string cres_name = res_central->GetResName();
      std::pair<bool, dictionary_residue_restraints_t> d =
	 geom_p->get_monomer_restraints(cres_name, protein_geometry::IMOL_ENC_ANY);
      if (! d.first) {
	 std::cout << "Failed to get dictionary for " << cres_name << std::endl;
      } else {
	 // Happy path
	 central_residue_dictionary = d.second;

	 if (false)
	    std::cout << "central_residue_dictionary has " << central_residue_dictionary.atom_info.size()
		      << " atoms and " << central_residue_dictionary.bond_restraint.size()
		      << " bond restraints " << std::endl;

	 neighb_dictionaries.resize(neighbours.size());
	 have_dictionary = true;
	 for (unsigned int i=0; i<neighbours.size(); i++) {
	    std::string residue_name = neighbours[i]->GetResName();
	    d = geom_p->get_monomer_restraints(residue_name, protein_geometry::IMOL_ENC_ANY);
	    if (! d.first) {
	       std::cout << "WARNING:: Overlap fail. Failed to get dictionary for name "
			 << residue_name << std::endl;
	       have_dictionary = false;
	       break;
	    } else {
	       // happy path
	       neighb_dictionaries[i] = d.second;
	    }
	 }
      }

      if (have_dictionary) {
	 fill_ligand_atom_neighbour_map(); // and add radius
	 mark_donors_and_acceptors();
      }
   }
}

void
coot::atom_overlaps_container_t::init_for_all_atom() {

   overlap_mode = ALL_ATOM;

   // neighbours is a misnomer in this case - it is merely a list of all residue

   have_dictionary = true; // initially.

   int imod = 1;
   mmdb::Model *model_p = mol->GetModel(imod);
   if (model_p) {
      mmdb::Chain *chain_p;
      int n_chains = model_p->GetNumberOfChains();
      for (int ichain=0; ichain<n_chains; ichain++) {
	 chain_p = model_p->GetChain(ichain);
	 int nres = chain_p->GetNumberOfResidues();
	 mmdb::Residue *residue_p;
	 for (int ires=0; ires<nres; ires++) {
	    residue_p = chain_p->GetResidue(ires);
	    if (residue_p) {
	       neighbours.push_back(residue_p);
	       std::string residue_name(residue_p->GetResName());
	       std::map<std::string, dictionary_residue_restraints_t>::const_iterator it;
	       it = dictionary_map.find(residue_name);
	       if (it == dictionary_map.end()) {
		  std::pair<bool, dictionary_residue_restraints_t> d =
		     geom_p->get_monomer_restraints(residue_name, protein_geometry::IMOL_ENC_ANY);
		  if (! d.first) {
		     std::cout << "Failed to get dictionary for " << residue_name << std::endl;
		     have_dictionary = false;
		  } else {
		     dictionary_map[residue_name] = d.second;
		  }
	       }
	    }
	 }
      }
   }

   // now mark donors and acceptors.
   //
   udd_h_bond_type_handle = mol->RegisterUDInteger(mmdb::UDR_ATOM, "hb_type");

   mark_donors_and_acceptors_for_neighbours(udd_h_bond_type_handle);

}

void
coot::atom_overlaps_container_t::mark_donors_and_acceptors() {

   // now mark donors and acceptors.
   //
   udd_h_bond_type_handle = mol->RegisterUDInteger(mmdb::UDR_ATOM, "hb_type");

   mark_donors_and_acceptors_central_residue(udd_h_bond_type_handle);

   mark_donors_and_acceptors_for_neighbours(udd_h_bond_type_handle);
}

void
coot::atom_overlaps_container_t::mark_donors_and_acceptors_central_residue(int udd_h_bond_type_handle) {

   if (res_central) {
      mmdb::PAtom *central_residue_atoms = 0;
      int n_central_residue_atoms;
      res_central->GetAtomTable(central_residue_atoms, n_central_residue_atoms);
      for (int iat=0; iat<n_central_residue_atoms; iat++) {
	 mmdb::Atom *at = central_residue_atoms[iat];
	 std::string atom_name(at->name);
	 std::string ele = at->element;
	 if (ele == " H") {
	    // Hydrogens have energy type "H" from Refmac and acedrg, that doesn't
	    // tell us if this atom is a donor hydrogen.
	    // So, find the atom to which the H is attached and if that is a donor then this
	    // is a hydrogen bond hydrogen.
	    std::string heavy_neighb_of_H_atom =
	       central_residue_dictionary.get_bonded_atom(atom_name);
	    if (! heavy_neighb_of_H_atom.empty()) {
	       std::string neigh_energy_type = central_residue_dictionary.type_energy(heavy_neighb_of_H_atom);
	       energy_lib_atom neighb_ela = geom_p->get_energy_lib_atom(neigh_energy_type);
	       hb_t neighb_hb_type = neighb_ela.hb_type;
	       if (neighb_hb_type == coot::HB_DONOR) {
		  // std::cout << "----- adding ligand HB_HYDROGEN udd " << atom_spec_t(at) << std::endl;
		  at->PutUDData(udd_h_bond_type_handle, coot::HB_HYDROGEN); // hb_t -> int
	       }
	       if (neighb_hb_type == coot::HB_BOTH) {
		  // std::cout << "----- adding ligand HB_HYDROGEN udd " << atom_spec_t(at) << std::endl;
		  at->PutUDData(udd_h_bond_type_handle, coot::HB_HYDROGEN); // hb_t -> int
	       }
	    }
	 } else {
	    std::string energy_type = central_residue_dictionary.type_energy(atom_name);
	    energy_lib_atom ela = geom_p->get_energy_lib_atom(energy_type);
	    hb_t hb_type = ela.hb_type;
	    at->PutUDData(udd_h_bond_type_handle, hb_type); // hb_t -> int
	 }
      }
   }
}

const coot::dictionary_residue_restraints_t &
coot::atom_overlaps_container_t::get_dictionary(mmdb::Residue *r, unsigned int idx) const {

   if (overlap_mode == ALL_ATOM) {
      std::string res_name = r->GetResName();
      std::map<std::string, dictionary_residue_restraints_t>::const_iterator it =
	 dictionary_map.find(res_name);

      if (it == dictionary_map.end()) {
	 std::cout << "========= hideous failure in get_dictionary for type " << res_name
		   << " using " << dictionary_map.size() << " dictionary entries" << std::endl;
      }
      return it->second;
   } else {
      return neighb_dictionaries[idx];
   }
}

void
coot::atom_overlaps_container_t::mark_donors_and_acceptors_for_neighbours(int udd_h_bond_type_handle) {

   for (unsigned int i=0; i<neighbours.size(); i++) {
      // const dictionary_residue_restraints_t &dict = neighb_dictionaries[i];
      const dictionary_residue_restraints_t &dict = get_dictionary(neighbours[i], i);
      mmdb::PAtom *residue_atoms = 0;
      int n_residue_atoms;
      neighbours[i]->GetAtomTable(residue_atoms, n_residue_atoms);
      for (int iat=0; iat<n_residue_atoms; iat++) { 
	 mmdb::Atom *n_at = residue_atoms[iat];
	 std::string atom_name(n_at->name);
	 std::string ele = n_at->element;
	 if (ele == " H") {
	    // as above
	    std::string heavy_neighb_of_H_atom = dict.get_bonded_atom(atom_name);
	    if (! heavy_neighb_of_H_atom.empty()) {
	       std::string neigh_energy_type = dict.type_energy(heavy_neighb_of_H_atom);
	       energy_lib_atom neighb_ela = geom_p->get_energy_lib_atom(neigh_energy_type);
	       hb_t neighb_hb_type = neighb_ela.hb_type;
	       if (neighb_hb_type == coot::HB_DONOR) {
		  // std::cout << "----- adding env HB_HYDROGEN udd " << atom_spec_t(n_at) << std::endl;
		  n_at->PutUDData(udd_h_bond_type_handle, coot::HB_HYDROGEN); // hb_t -> int
	       }
	       if (neighb_hb_type == coot::HB_BOTH) {
		  // std::cout << "----- adding env HB_HYDROGEN udd " << atom_spec_t(n_at) << std::endl;
		  n_at->PutUDData(udd_h_bond_type_handle, coot::HB_HYDROGEN); // hb_t -> int
	       }
	    }
	 } else {
	    std::string atom_name(n_at->name);
	    if (false)
	       std::cout << "........... looking up type_energy for atom name " << atom_name
			 << " in dictionary for comp-id \"" << dict.residue_info.comp_id
			 << "\"" << std::endl;
	    std::string energy_type = dict.type_energy(atom_name);
	    energy_lib_atom ela = geom_p->get_energy_lib_atom(energy_type);
	    hb_t hb_type = ela.hb_type;
	    n_at->PutUDData(udd_h_bond_type_handle, hb_type); // hb_t -> int
	 }
      }
   }
}

// and radius
void
coot::atom_overlaps_container_t::fill_ligand_atom_neighbour_map() {

   mmdb::realtype max_dist = 2.3;
   if (mol) {
      mmdb::Contact *pscontact = NULL;
      int n_contacts;
      float min_dist = 0.01;
      long i_contact_group = 1;
      mmdb::mat44 my_matt;
      mmdb::SymOps symm;
      for (int i=0; i<4; i++)
	 for (int j=0; j<4; j++)
	    my_matt[i][j] = 0.0;
      for (int i=0; i<4; i++) my_matt[i][i] = 1.0;

      mmdb::Atom **residue_atoms = 0;
      int n_residue_atoms;
      res_central->GetAtomTable(residue_atoms, n_residue_atoms);

      mol->SeekContacts(residue_atoms, n_residue_atoms,
			residue_atoms, n_residue_atoms,
			0, max_dist,
			0, // in same residue
			pscontact, n_contacts,
			0, &my_matt, i_contact_group); // makes reverses also
      if (n_contacts > 0) {
	 if (pscontact) {
	    for (int i=0; i<n_contacts; i++) {
	       mmdb::Atom *neighb_at = residue_atoms[pscontact[i].id2];
	       double radius = get_vdw_radius_ligand_atom(neighb_at);
	       std::pair<mmdb::Atom *, double> p(neighb_at, radius);
	       ligand_atom_neighbour_map[pscontact[i].id1].push_back(p);
	    }
	 }
      }
   }
}




void
coot::atom_overlaps_container_t::make_overlaps() {

   if (! have_dictionary) {
      std::cout << "No dictionary" << std::endl;
      return;
   }

   // This is completely non-clever about finding which atoms are close
   // to each other - it checks all neighbour atoms against all
   // central-residue atoms.

   double dist_crit = 2.3; // Everything that is bonded is less than this
   double dist_crit_sqrd = (2*dist_crit) * (2*dist_crit);

   mmdb::PAtom *central_residue_atoms = 0;
   int n_central_residue_atoms;
   res_central->GetAtomTable(central_residue_atoms, n_central_residue_atoms);

   for (int j=0; j<n_central_residue_atoms; j++) {

      mmdb::Atom *cr_at = central_residue_atoms[j];
      // std::cout << "Surface points for ligand atom " << atom_spec_t(cr_at) << std::endl;
      clipper::Coord_orth co_cr_at = co(cr_at);
      double r_1 = get_vdw_radius_ligand_atom(cr_at);

      for (unsigned int i=0; i<neighbours.size(); i++) { 
	 mmdb::PAtom *residue_atoms = 0;
	 int n_residue_atoms;
	 neighbours[i]->GetAtomTable(residue_atoms, n_residue_atoms);
	 for (int iat=0; iat<n_residue_atoms; iat++) { 
	    mmdb::Atom *n_at = residue_atoms[iat];
	    clipper::Coord_orth co_n_at = co(n_at);

	    double ds = (co_cr_at - co_n_at).lengthsq();
	    if (ds < dist_crit_sqrd) {

	       double r_2 = get_vdw_radius_neighb_atom(n_at, i);
	       double d = sqrt(ds);

	       // first is yes/no, second is if the H is on the ligand
	       // 
	       std::pair<bool, bool> might_be_h_bond_flag = is_h_bond_H_and_acceptor(cr_at, n_at);

	       if (d < (r_1 + r_2 + probe_radius)) { 
		  double o = get_overlap_volume(d, r_2, r_1);
		  bool h_bond_flag = false;
		  if (might_be_h_bond_flag.first) {
		     h_bond_flag = true;
		     if (might_be_h_bond_flag.second) { 
			std::cout << atom_spec_t(cr_at) << "   " << " and " << atom_spec_t(n_at)
				  << " r_1 " << r_1 << " and r_2 " << r_2  <<  " and d " << d 
				  << " overlap " << o << " IS H-Bond (ligand donor)" << std::endl;
		     } else { 
			   std::cout << atom_spec_t(cr_at) << "   " << " and " << atom_spec_t(n_at)
				     << " r_1 " << r_1 << " and r_2 " << r_2  <<  " and d " << d 
				     << " overlap " << o << " IS H-Bond (ligand acceptor)" << std::endl;
			   
		     }
		  } else { 
		     std::cout << atom_spec_t(cr_at) << "   " << " and " << atom_spec_t(n_at)
			       << " r_1 " << r_1 << " and r_2 " << r_2  <<  " and d " << d 
			       << " overlap " << o << std::endl;
		  }
		  atom_overlap_t ao(j, cr_at, n_at, r_1, r_2, o);
		  // atom_overlap_t ao2(-1, n_at, cr_at, r_2, r_1, o);
		  ao.is_h_bond = h_bond_flag;
		  overlaps.push_back(ao);
		  // overlaps.push_back(ao2);
	       } else {
		  if (might_be_h_bond_flag.first) { 
		     if (d < (dist_crit + 0.5)) {
			std::cout << atom_spec_t(cr_at) << "   " << " and " << atom_spec_t(n_at)
				  << " r_1 " << r_1 << " and r_2 " << r_2  <<  " and d " << d
				  << " but might be h-bond anyway (is this strange?)"
				  << std::endl;
			double o = 0;
			// atom_overlap_t ao(cr_at, n_at, r_1, r_2, o);
			// ao.is_h_bond = true;
			// overlaps.push_back(ao);
		     }
		  }
	       }
	    }
	 }
      }
   }
}

// first is yes/no, second is if the H is on the ligand
//
// also allow water to be return true values
//
std::pair<bool, bool>
coot::atom_overlaps_container_t::is_h_bond_H_and_acceptor(mmdb::Atom *ligand_atom,
							  mmdb::Atom *env_atom
							  // const double &d
							  ) const {

   bool status = false;
   bool H_on_ligand = false;
   unsigned int n_h = 0;
   bool h_on_ligand_atom = false;
   bool h_on_env_atom    = false;

   int hb_1 = -1;
   int hb_2 = -1;
   if (ligand_atom->GetUDData(udd_h_bond_type_handle, hb_1) == mmdb::UDDATA_Ok) { 
      if (env_atom->GetUDData(udd_h_bond_type_handle, hb_2) == mmdb::UDDATA_Ok) {

	 if (false) // testing
	    std::cout << "    hb_1 " << hb_1 << " for " << coot::atom_spec_t(ligand_atom)
		      << " and hb_2 " << hb_2 << " for neighb-atom " << coot::atom_spec_t(env_atom)
		      << std::endl;

	 if (hb_1 == HB_HYDROGEN) {
	    if (hb_2 == HB_ACCEPTOR || hb_2 == HB_BOTH) {
	       status = true;
	       H_on_ligand = true;
	    }
	 } 

	 if (hb_1 == HB_ACCEPTOR || hb_1 == HB_BOTH) {
	    if (hb_2 == HB_HYDROGEN) {
	       status = true;
	       H_on_ligand = false;
	    }
	 }
      } else {
	 // it's not bad.  Some H atoms dont have hydrogen UDD.
	 if (false)
	    std::cout << "   bad get of uud_h_bond info for env atom " << coot::atom_spec_t(env_atom)
		      << std::endl;
      }

      if (status == false) {
	 // allow HOH to H-bond
	 std::string resname_1 = ligand_atom->GetResName();
	 std::string resname_2 = env_atom->GetResName();
	 if (resname_1 == "HOH")
	    if (hb_2 == HB_ACCEPTOR || hb_2 == HB_DONOR || hb_2 == HB_BOTH || hb_2 == HB_HYDROGEN)
	       status = true;
	 if (resname_2 == "HOH")
	    if (hb_1 == HB_ACCEPTOR || hb_1 == HB_DONOR || hb_1 == HB_BOTH || hb_1 == HB_HYDROGEN)
	       status = true;
      }
   } else {
      if (false)
	 std::cout << "   bad get of uud_h_bond info for ligand atom " << coot::atom_spec_t(ligand_atom)
		   << std::endl;
   }
   return std::pair<bool, bool> (status, H_on_ligand);
}

// in A^3
double
coot::atom_overlaps_container_t::get_overlap_volume(const double &d, const double &r_1, const double &r_2) const {

   // V = π /(12d) * (r1+r2−d) ^2 * (d^2+2d(r1+r2)−3*(r1−r2)^2)

   double V = (M_PI/(12*d)) * (r_1+r_2-d) * (r_1+r_2-d) * (d*d + 2.0*d*(r_1+r_2) - 3*(r_1-r_2)*(r_1-r_2));
   return V;
} 

double
coot::atom_overlaps_container_t::get_vdw_radius_ligand_atom(mmdb::Atom *at) {

   double r = 2.5;

   std::map<mmdb::Atom *, double>::const_iterator it = central_residue_atoms_vdw_radius_map.find(at);
   if (it == central_residue_atoms_vdw_radius_map.end()) {
      // we need to add it then

      // What's the energy type of Atom at?
      //
      // PDBv3 FIXME - change from 4-char
      std::string te = central_residue_dictionary.type_energy(at->GetAtomName());
      if (! te.empty()) {
	 std::map<std::string, double>::const_iterator it_type =
	    type_to_vdw_radius_map.find(te);
	 if (it_type == type_to_vdw_radius_map.end()) {
	    // didn't find it. so look it up and add it.
	    r = geom_p->get_energy_lib_atom(te).vdw_radius;
	    hb_t t = geom_p->get_energy_lib_atom(te).hb_type;

	    if (false)
	       std::cout << "setting map: type_to_vdw_radius_h_bond_type_map[" << te << "] to ("
			 << r << "," << t << ")" << std::endl;

	    type_to_vdw_radius_map[te] = r;
	 } else {
	    r = it_type->second;
	 }
	 central_residue_atoms_vdw_radius_map[at] = r;
      } else {
	 std::cout << "failed to find type-energy for atom " << atom_spec_t(at) << std::endl;
      }
   } else {
      if (false)
	 std::cout << "radius for atom " << atom_spec_t(at) << " was found in map: value: "
		   << it->second << std::endl;
      r = it->second;
   }

   return r;
}


double
coot::atom_overlaps_container_t::get_vdw_radius_neighb_atom(int idx_neigh_atom) const {

   // no index checking (a bit cowboy?)
   //
   double r = neighb_atom_radius[idx_neigh_atom];
   return r;

}

double
coot::atom_overlaps_container_t::get_vdw_radius_neighb_atom(mmdb::Atom *at, unsigned int idx_res) {

   double r = 1.5;

   std::map<mmdb::Atom *, double>::const_iterator it = neighbour_atoms_vdw_radius_map.find(at);
   if (it == neighbour_atoms_vdw_radius_map.end()) {
      // we need to add it then

      // What's the energy type of Atom at?
      //
      std::string te = neighb_dictionaries[idx_res].type_energy(at->GetAtomName());
      std::map<std::string, double>::const_iterator it_type = type_to_vdw_radius_map.find(te);
      if (it_type == type_to_vdw_radius_map.end()) {
	 // didn't find te in types map. so look it up from the dictionary and add to the types map
	 r = geom_p->get_energy_lib_atom(te).vdw_radius;
	 hb_t t = geom_p->get_energy_lib_atom(te).hb_type;
	 type_to_vdw_radius_map[te] = r;
      } else {
	 r = it_type->second;
      }
      neighbour_atoms_vdw_radius_map[at] = r;
   } else {
      r = it->second;
   }

   return r;
} 



coot::hb_t 
coot::atom_overlaps_container_t::get_h_bond_type(mmdb::Atom *at) {

   hb_t type = HB_UNASSIGNED;
   std::string atom_name = at->name;
   std::string res_name = at->GetResName();
   type = geom_p->get_h_bond_type(atom_name, res_name, protein_geometry::IMOL_ENC_ANY); // heavyweight

   return type;
} 



// probably not the function that you're looking for.
// Maybe delete this?
void
coot::atom_overlaps_container_t::contact_dots_for_overlaps() const {

   double spike_length = 0.5;
   double clash_dist = 0.4;

   double dot_density = 0.35;
   dot_density = 1.0;

   double   phi_step = 5.0 * (M_PI/180.0);
   double theta_step = 5.0 * (M_PI/180.0);
   if (dot_density > 0.0) {
      phi_step   /= dot_density;
      theta_step /= dot_density;
   }

   for (unsigned int i=0; i<overlaps.size(); i++) {

      // std::cout << "considering overlap idx: " << i << std::endl;

      clipper::Coord_orth pt_at_1(overlaps[i].atom_1->x,
				  overlaps[i].atom_1->y,
				  overlaps[i].atom_1->z);
      clipper::Coord_orth pt_at_2(overlaps[i].atom_2->x,
				  overlaps[i].atom_2->y,
				  overlaps[i].atom_2->z);
      const double &r_1 = overlaps[i].r_1;
      const double &r_2 = overlaps[i].r_2;
      const int &idx =    overlaps[i].ligand_atom_index;
      double r_2_sqrd = r_2 * r_2;
      double r_2_plus_prb_squard  = r_2_sqrd + 2 * r_2 * probe_radius + probe_radius * probe_radius;
      double r_2_minux_prb_squard = r_2_sqrd - 2 * r_2 * probe_radius + probe_radius * probe_radius;
      //
      bool done_atom_name = false;

      bool even = true;
      for (double theta=0; theta<M_PI; theta+=theta_step) {
	 double phi_step_inner = phi_step + 0.1 * pow(theta-0.5*M_PI, 2);
	 for (double phi=0; phi<2*M_PI; phi+=phi_step_inner) {
	    if (even) {
	       clipper::Coord_orth pt(r_1*cos(phi)*sin(theta),
				      r_1*sin(phi)*sin(theta),
				      r_1*cos(theta));
	       clipper::Coord_orth pt_at_surface = pt + pt_at_1;
	       double d_sqrd = (pt_at_2 - pt_at_surface).lengthsq();

	       // std::cout << "comparing " << sqrt(d_sqrd) << " vs " << sqrt(r_2_plus_prb_squard)
	       // << " with r_2 " << r_2 << std::endl;

	       if (d_sqrd > r_2_plus_prb_squard) {

		  bool draw_it = ! is_inside_another_ligand_atom(idx, pt_at_surface);

		  if (false) // debugging
		     if (std::string(overlaps[i].atom_1->name) != " HO3")
			draw_it = false;

		  if (draw_it) {

		     std::string type = "wide-contact";
		     bool only_pt = true; // not a spike

		     if (d_sqrd < r_2_sqrd)
			type = "close-contact";

		     if (d_sqrd < (r_2_sqrd - 2 * r_2 * clash_dist + clash_dist * clash_dist)) {
			type = "clash";
			only_pt = false;
		     }

		     if (overlaps[i].is_h_bond) {
			type = "H-bond";
		     }

		     if (false) // debugging
			if (! done_atom_name) {
			   std::cout << "   spike for atom " << coot::atom_spec_t(overlaps[i].atom_1)
				     << std::endl;
			   done_atom_name = true;
			}

		     clipper::Coord_orth pt_spike_inner = pt_at_surface;
		     if (! only_pt) {
			std::cout << "considering overlap idx: " << i << " "
				  << atom_spec_t(overlaps[i].atom_1) << " to "
				  << atom_spec_t(overlaps[i].atom_2) << std::endl;

			clipper::Coord_orth vect_to_pt_1 = pt_at_1 - pt_at_surface;
			clipper::Coord_orth vect_to_pt_1_unit(vect_to_pt_1.unit());

			// these days, spikes project away from the atom, not inwards
			//
			// clipper::Coord_orth pt_spike_inner = pt_at_surface + spike_length * vect_to_pt_1_unit;
			pt_spike_inner = pt_at_surface - spike_length * vect_to_pt_1_unit;
		     }

		     // on the surface of atom_1 inside the sphere of atom_2
		     std::cout << "spike "
			       << type << " "
			       << pt_at_surface.x() << " "
			       << pt_at_surface.y() << " "
			       << pt_at_surface.z() << " to "
			       << pt_spike_inner.x() << " "
			       << pt_spike_inner.y() << " "
			       << pt_spike_inner.z()
			       << " theta " << theta << " phi " << phi
			       << std::endl;
		  }
	       }
	    }
	 }
      }
   }
}

bool
coot::atom_overlaps_container_t::is_inside_another_ligand_atom(int idx,
							       const clipper::Coord_orth &dot_pt) const {
   bool r = false;

   if (idx >= 0) {

      const std::vector<std::pair<mmdb::Atom *, double> > &v = ligand_atom_neighbour_map.find(idx)->second;
      for (unsigned int i=0; i<v.size(); i++) {
	 clipper::Coord_orth pt = co(v[i].first);
	 double dist_sqrd = (dot_pt - pt).lengthsq();

	 const double &radius_other = v[i].second;
	 if (dist_sqrd < radius_other * radius_other) {
	    r = true;
	    break;
	 }
      }
   }
   return r;
}
bool
coot::atom_overlaps_container_t::is_inside_another_ligand_atom(int idx,
							       const clipper::Coord_orth &probe_pos,
							       const clipper::Coord_orth &dot_pt) const {

   // for better speed, change the atom -> radius map to atom index -> radius vector
   // (and directly look up the radius).

   bool consider_probe_also = false; // if this is true, then don't make surface points for inner cusps
   
   bool r = false;

   if (idx >= 0) {

      const std::vector<std::pair<mmdb::Atom *, double> > &v = ligand_atom_neighbour_map.find(idx)->second;
      for (unsigned int i=0; i<v.size(); i++) {
	 clipper::Coord_orth pt = co(v[i].first);
	 double dist_sqrd = (dot_pt - pt).lengthsq(); // should be r_1 squared, right?
	 double radius_other = v[i].second;
	 radius_other += probe_radius;
	 if (dist_sqrd < radius_other * radius_other) {
	    r = true;
	    break;
	 }
      }
   }
   return r;
}


coot::atom_overlaps_dots_container_t
coot::atom_overlaps_container_t::contact_dots_for_ligand() { // or residue

   atom_overlaps_dots_container_t ao;
   mmdb::realtype max_dist = 4.0; // max distance for an interaction
   if (mol) {
      mmdb::Contact *pscontact = NULL;
      int n_contacts;
      float min_dist = 0.01;
      long i_contact_group = 1;
      mmdb::mat44 my_matt;
      mmdb::SymOps symm;
      for (int i=0; i<4; i++)
	 for (int j=0; j<4; j++)
	    my_matt[i][j] = 0.0;
      for (int i=0; i<4; i++) my_matt[i][i] = 1.0;

      mmdb::Atom **ligand_residue_atoms = 0;
      int n_ligand_residue_atoms;
      res_central->GetAtomTable(ligand_residue_atoms, n_ligand_residue_atoms);

      std::vector<residue_spec_t> env_residue_specs(neighbours.size());
      for (unsigned int i=0; i<neighbours.size(); i++)
	 env_residue_specs[i] = residue_spec_t(neighbours[i]);
      add_residue_neighbour_index_to_neighbour_atoms();

      int mask_mode = 0; // all atoms
      int i_sel_hnd_env_atoms = specs_to_atom_selection(env_residue_specs, mol, mask_mode);

      mmdb::Atom **env_residue_atoms = 0;
      int n_env_residue_atoms;
      mol->GetSelIndex(i_sel_hnd_env_atoms, env_residue_atoms, n_env_residue_atoms);
      setup_env_residue_atoms_radii(i_sel_hnd_env_atoms);

      mol->SeekContacts(ligand_residue_atoms, n_ligand_residue_atoms,
			env_residue_atoms, n_env_residue_atoms,
			0, max_dist,
			1, // 0: in same residue also?
			pscontact, n_contacts,
			0, &my_matt, i_contact_group);

      if (n_contacts > 0) {
	 if (pscontact) {

	    // which atoms are close to which other atoms?
	    std::map<int, std::vector<int> > contact_map; // these atoms can have nbc interactions
	    std::map<int, std::vector<int> > bonded_map;  // these atoms are bonded and can mask

	    // the dots of an atom
	    // which atom names of which residues are bonded or 1-3 related? (update the map
	    // as you find and add new residue types in bonded_angle_or_ring_related().
	    std::map<std::string, std::vector<std::pair<std::string, std::string> > > bonded_neighbours;
	    // similar thinking: update the ring list map
	    std::map<std::string, std::vector<std::vector<std::string> > > ring_list_map;

	    for (int i=0; i<n_contacts; i++) {
	       bool mc_flag = true;
	       atom_interaction_type ait =
		  bonded_angle_or_ring_related(mol, // also check links
					       ligand_residue_atoms[pscontact[i].id1],
					       env_residue_atoms[pscontact[i].id2], mc_flag,
					       bonded_neighbours,   // updatedby fn.
					       ring_list_map        // updatedby fn.
					       );
	       if (ait == CLASHABLE) {
		  contact_map[pscontact[i].id1].push_back(pscontact[i].id2);
	       } else {
		  if (ait == BONDED) {
		     bonded_map[pscontact[i].id1].push_back(pscontact[i].id2);
		  }
	       }
	    }
	    std::cout << "done contact map" << std::endl;


	    for (int iat=0; iat<n_ligand_residue_atoms; iat++) {

	       mmdb::Atom *cr_at = ligand_residue_atoms[iat];
	       clipper::Coord_orth pt_at_1 = co(cr_at);

	       double dot_density = 0.35;
	       dot_density = 1.05;
	       if (std::string(cr_at->element) == " H")
		  dot_density *=0.66; // so that surface dots on H atoms don't appear (weirdly) more fine

	       double   phi_step = 5.0 * (M_PI/180.0);
	       double theta_step = 5.0 * (M_PI/180.0);
	       if (dot_density > 0.0) {
		  phi_step   /= dot_density;
		  theta_step /= dot_density;
	       }
	       double r_1 = get_vdw_radius_ligand_atom(cr_at);
	       bool even = true;
	       for (double theta=0; theta<M_PI; theta+=theta_step) {
		  double phi_step_inner = phi_step + 0.1 * pow(theta-0.5*M_PI, 2);
		  for (double phi=0; phi<2*M_PI; phi+=phi_step_inner) {
		     if (even) {
			clipper::Coord_orth pt(r_1*cos(phi)*sin(theta),
					       r_1*sin(phi)*sin(theta),
					       r_1*cos(theta));
			clipper::Coord_orth pt_at_surface = pt + pt_at_1;
			bool draw_it = ! is_inside_another_ligand_atom(iat, pt_at_surface);

			if (draw_it) {

			   // is the point on the surface of cr_at inside the sphere
			   // of an environment atom?
			   // If so, we want to know which one to which it was closest

			   double biggest_overlap = -1; // should be positive if we get a hit
			   mmdb::Atom *atom_with_biggest_overlap = 0;
			   double r_2_for_biggest_overlap = 0;

			   // now check which atom this is clashing with (if any) and pick the
			   // one with the biggest overlap
			   //
			   const std::vector<int> &v = contact_map[iat];
			   for (unsigned int jj=0; jj<v.size(); jj++) {

			      mmdb::Atom *neighb_atom = env_residue_atoms[v[jj]];
			      double r_2 = get_vdw_radius_neighb_atom(v[jj]);
			      double r_2_sqrd = r_2 * r_2;
			      double r_2_plus_prb_squard =
				 r_2_sqrd + 2 * r_2 * probe_radius + probe_radius * probe_radius;

			      clipper::Coord_orth pt_na = co(neighb_atom);
			      double d_sqrd = (pt_na - pt_at_surface).lengthsq();

			      if (false)
				 std::cout << " for atom "
					   << atom_spec_t(env_residue_atoms[v[jj]])
					   << " comparing " << d_sqrd << " vs "
					   << r_2_plus_prb_squard << " with r_2 " << r_2
					   << " probe_radius " << probe_radius << std::endl;

			      if (d_sqrd < r_2_plus_prb_squard) {

				 // OK it was close to something.
				 double delta_d_sqrd = r_2_plus_prb_squard - d_sqrd;
				 if (delta_d_sqrd > biggest_overlap) {
				    biggest_overlap = delta_d_sqrd;
				    atom_with_biggest_overlap = neighb_atom;
				    r_2_for_biggest_overlap = r_2;
				 }
			      }
			   }

			   if (atom_with_biggest_overlap) {
			      double d_surface_pt_to_atom_sqrd =
				 (co(atom_with_biggest_overlap)-pt_at_surface).lengthsq();
			      double d_surface_pt_to_atom = sqrt(d_surface_pt_to_atom_sqrd);
			      double overlap_delta = r_2_for_biggest_overlap - d_surface_pt_to_atom;

			      // first is yes/no, second is H-is-on-ligand?
			      std::pair<bool, bool> might_be_h_bond_flag =
				 is_h_bond_H_and_acceptor(cr_at, atom_with_biggest_overlap);
			      bool is_h_bond = false;
			      if (might_be_h_bond_flag.first)
				 is_h_bond = true;
			      std::string c_type = overlap_delta_to_contact_type(overlap_delta, is_h_bond);

			      if (false)
				 std::cout << "spike "
					   << c_type << " "
					   << pt_at_surface.x() << " "
					   << pt_at_surface.y() << " "
					   << pt_at_surface.z() << " to "
					   << pt_at_surface.x() << " "
					   << pt_at_surface.y() << " "
					   << pt_at_surface.z()
					   << " theta " << theta << " phi " << phi
					   << std::endl;

			      if (c_type != "clash") {
				 ao.dots[c_type].push_back(pt_at_surface);
			      } else {
				 clipper::Coord_orth vect_to_pt_1 = pt_at_1 - pt_at_surface;
				 clipper::Coord_orth vect_to_pt_1_unit(vect_to_pt_1.unit());
				 // these days, spikes project away from the atom, not inwards
				 // clipper::Coord_orth pt_spike_inner =
				 // pt_at_surface + clash_spike_length * vect_to_pt_1_unit;
				 clipper::Coord_orth pt_spike_inner =
				    pt_at_surface -
				    0.34* sqrt(biggest_overlap) * clash_spike_length * vect_to_pt_1_unit;
				 std::pair<clipper::Coord_orth, clipper::Coord_orth> p(pt_at_surface,
										       pt_spike_inner);
				 ao.clashes.positions.push_back(p);
			      }

			   } else {

			      // no environment atom was close to this ligand atom, so just add
			      // a surface point

			      if (false)
				 std::cout << "spike-surface "
					   << pt_at_surface.x() << " "
					   << pt_at_surface.y() << " "
					   << pt_at_surface.z() << std::endl;

			      ao.dots["vdw-surface"].push_back(pt_at_surface);
			   }
			}
		     }
		  }
	       }
	    }
	 }
      }
   }
   return ao;
}

coot::atom_overlaps_dots_container_t
coot::atom_overlaps_container_t::all_atom_contact_dots(double dot_density_in) {

   coot::atom_overlaps_dots_container_t ao;

   if (mol) {
      std::cout << " in all_atom_contact_dots() with mol " << std::endl;
      mmdb::realtype max_dist = 1.75 + 1.75 + probe_radius; // max distance for an interaction
      mmdb::realtype min_dist = 0.01;
      int i_sel_hnd = mol->NewSelection();
      mol->SelectAtoms (i_sel_hnd, 0, "*",
			mmdb::ANY_RES, // starting resno, an int
			"*", // any insertion code
			mmdb::ANY_RES, // ending resno
			"*", // ending insertion code
			"*", // any residue name
			"*", // atom name
			"*", // elements
			"*"  // alt loc.
			);
      ao = all_atom_contact_dots_internal(dot_density_in, mol, i_sel_hnd, i_sel_hnd, min_dist, max_dist);
   }
   return ao;
}

      
coot::atom_overlaps_dots_container_t
coot::atom_overlaps_container_t::all_atom_contact_dots_internal(double dot_density_in,
								mmdb::Manager *mol,
								int i_sel_hnd_1,
								int i_sel_hnd_2,
								mmdb::realtype min_dist,
								mmdb::realtype max_dist) {
   
   coot::atom_overlaps_dots_container_t ao;

   long i_contact_group = 1;
   mmdb::mat44 my_matt;
   mmdb::SymOps symm;
   for (int i=0; i<4; i++)
      for (int j=0; j<4; j++)
	 my_matt[i][j] = 0.0;
   for (int i=0; i<4; i++) my_matt[i][i] = 1.0;

   mmdb::Atom **atom_selection = 0;
   int n_selected_atoms;
   mol->GetSelIndex(i_sel_hnd_1, atom_selection, n_selected_atoms);
   setup_env_residue_atoms_radii(i_sel_hnd_1);
   mmdb::Contact *pscontact = NULL;
   int n_contacts;
   mol->SeekContacts(atom_selection, n_selected_atoms,
		     atom_selection, n_selected_atoms,
		     0.01, max_dist,
		     0, // 0: in same residue also?
		     pscontact, n_contacts,
		     0, &my_matt, i_contact_group);
   std::cout << "found " << n_selected_atoms << " selected atoms" << std::endl;
   std::cout << "found " << n_contacts << " all-atom contacts" << std::endl;
   if (n_contacts > 0) {
      if (pscontact) {
	 // which atoms are close to which other atoms?
	 std::map<int, std::vector<int> > contact_map; // these atoms can have nbc interactions
	 std::map<int, std::vector<int> > bonded_map;  // these atoms are bonded and can mask
	 // the dots of an atom
	 // which atom names of which residues are bonded or 1-3 related? (update the map
	 // as you find and add new residue types in bonded_angle_or_ring_related().
	 std::map<std::string, std::vector<std::pair<std::string, std::string> > > bonded_neighbours;
	 // similar thinking: update the ring list map
	 std::map<std::string, std::vector<std::vector<std::string> > > ring_list_map;

	 for (int i=0; i<n_contacts; i++) {
	    bool mc_flag = true;
	    atom_interaction_type ait =
	       bonded_angle_or_ring_related(mol, // also check links
					    atom_selection[pscontact[i].id1],
					    atom_selection[pscontact[i].id2], mc_flag,
					    bonded_neighbours,   // updatedby fn.
					    ring_list_map        // updatedby fn.
					    );
	    if (ait == CLASHABLE) {
	       contact_map[pscontact[i].id1].push_back(pscontact[i].id2);
	    } else {
	       if (ait == BONDED) {
		  bonded_map[pscontact[i].id1].push_back(pscontact[i].id2);
	       }
	    }
	 }

	 std::cout << "done contact map" << std::endl;

	 // atom_spec_t debug_spec("A", 477, "", " OE2", "");
	 atom_spec_t debug_spec("A", 523, "", " HE1", "");
	 atom_spec_t debug_spec_2("A", 480, "", " O  ", "");

	 for (int iat=0; iat<n_selected_atoms; iat++) {

	    mmdb::Atom *at = atom_selection[iat];

	    // if (!(atom_spec_t(at) == debug_spec))
	    // continue;
	    // if ((atom_spec_t(at) == debug_spec) || (atom_spec_t(at) == debug_spec_2)) {
	    // 		  // don't stop this atom
	    // } else {
	    // continue;
	    // }

	    clipper::Coord_orth pt_at_1 = co(at);
	    double dot_density = dot_density_in;
	    if (std::string(at->element) == " H")
	       dot_density *=0.66; // so that surface dots on H atoms don't appear (weirdly) more fine
	    double   phi_step = 5.0 * (M_PI/180.0);
	    double theta_step = 5.0 * (M_PI/180.0);
	    if (dot_density > 0.0) {
	       phi_step   /= dot_density;
	       theta_step /= dot_density;
	    }
	    double r_1 = get_vdw_radius_neighb_atom(iat);
	    bool even = true;
	    for (double theta=0; theta<M_PI; theta+=theta_step) {
	       double phi_step_inner = phi_step + 0.1 * pow(theta-0.5*M_PI, 2);
	       for (double phi=0; phi<2*M_PI; phi+=phi_step_inner) {
		  if (even) {
		     clipper::Coord_orth pt(r_1*cos(phi)*sin(theta),
					    r_1*sin(phi)*sin(theta),
					    r_1*cos(theta));
		     clipper::Coord_orth pt_at_surface = pt + pt_at_1;
		     bool draw_it = ! is_inside_another_atom_to_which_its_bonded(iat, at,
										 pt_at_surface,
										 bonded_map[iat],
										 atom_selection);

		     if (draw_it) {

			double biggest_overlap = -1; // should be positive if we get a hit
			mmdb::Atom *atom_with_biggest_overlap = 0;
			double r_2_for_biggest_overlap = 0;

			// now check which atom this is clashing with (if any) and pick the
			// one with the biggest overlap
			//
			const std::vector<int> &v = contact_map[iat];
			for (unsigned int jj=0; jj<v.size(); jj++) {
			   mmdb::Atom *neighb_atom = atom_selection[v[jj]];
			   double r_2 = get_vdw_radius_neighb_atom(v[jj]);
			   double r_2_sqrd = r_2 * r_2;
			   double r_2_plus_prb_squard = r_2_sqrd + 2 * r_2 * probe_radius +
			      probe_radius * probe_radius;
			   clipper::Coord_orth pt_na = co(neighb_atom);
			   double d_sqrd = (pt_na - pt_at_surface).lengthsq();

			   if (false)
			      std::cout << " for " << atom_spec_t(at) << " " << atom_spec_t(neighb_atom)
					<< " comparing " << d_sqrd << " vs " << r_2_plus_prb_squard
					<< std::endl;
			   if (d_sqrd < r_2_plus_prb_squard) {
			      // a contact dot on something
			      double delta_d_sqrd = r_2_plus_prb_squard - d_sqrd;
			      if (delta_d_sqrd > biggest_overlap) {
				 biggest_overlap = delta_d_sqrd;
				 atom_with_biggest_overlap = neighb_atom;
				 r_2_for_biggest_overlap = r_2;
			      }
			   }
			}

			if (atom_with_biggest_overlap) {
			   double d_surface_pt_to_atom_sqrd =
			      (co(atom_with_biggest_overlap) - pt_at_surface).lengthsq();
			   double d_surface_pt_to_atom = sqrt(d_surface_pt_to_atom_sqrd);
			   double overlap_delta = r_2_for_biggest_overlap - d_surface_pt_to_atom;
			   // first is yes/no, second is H-is-on-ligand?
			   // allow waters to H-bond (without being an H)
			   std::pair<bool, bool> might_be_h_bond_flag =
			      is_h_bond_H_and_acceptor(at, atom_with_biggest_overlap);
			   bool is_h_bond = false;
			   if (might_be_h_bond_flag.first)
			      is_h_bond = true;
			   std::string c_type = overlap_delta_to_contact_type(overlap_delta, is_h_bond);

			   clipper::Coord_orth pt_spike_inner = pt_at_surface;
			   if (c_type == "clash") {
			      clipper::Coord_orth vect_to_pt_1 = pt_at_1 - pt_at_surface;
			      clipper::Coord_orth vect_to_pt_1_unit(vect_to_pt_1.unit());
			      pt_spike_inner = pt_at_surface -
				 0.3 * sqrt(biggest_overlap) * clash_spike_length * vect_to_pt_1_unit;
			   }

			   // draw dot if these are atoms from different residues or this is not
			   // a wide contact
			   if ((at->residue != atom_with_biggest_overlap->residue) ||
			       (c_type != "wide-contact"))

			      if (false) // turn on for standard-out debugging
				 std::cout << "spike "
					   << c_type << " "
					   << pt_at_surface.x() << " "
					   << pt_at_surface.y() << " "
					   << pt_at_surface.z() << " to "
					   << pt_spike_inner.x() << " "
					   << pt_spike_inner.y() << " "
					   << pt_spike_inner.z()
					   << " theta " << theta << " phi " << phi
					   << std::endl;

			   if (c_type != "clash") {

			      // draw dot if these are atoms from different residues or this is not
			      // a wide contact
			      if ((at->residue != atom_with_biggest_overlap->residue) ||
				  (c_type != "wide-contact"))
				 ao.dots[c_type].push_back(pt_at_surface);
			   } else {
			      // clash
			      clipper::Coord_orth vect_to_pt_1 = pt_at_1 - pt_at_surface;
			      clipper::Coord_orth vect_to_pt_1_unit(vect_to_pt_1.unit());
			      // these days, spikes project away from the atom, not inwards
			      // clipper::Coord_orth pt_spike_inner =
			      // pt_at_surface + clash_spike_length * vect_to_pt_1_unit;
			      clipper::Coord_orth pt_spike_inner =
				 pt_at_surface -
				 0.34* sqrt(biggest_overlap) * clash_spike_length * vect_to_pt_1_unit;
			      std::pair<clipper::Coord_orth, clipper::Coord_orth> p(pt_at_surface,
										    pt_spike_inner);
			      ao.clashes.positions.push_back(p);
			   }

			} else {

			   // std::cout << "No atom_with_biggest_overlap" << std::endl;

			   // no environment atom was close to this ligand atom, so just add
			   // a surface point

			   if (false)
			      std::cout << "spike-surface "
					<< pt_at_surface.x() << " "
					<< pt_at_surface.y() << " "
					<< pt_at_surface.z() << std::endl;

			   ao.dots["vdw-surface"].push_back(pt_at_surface);
			}
		     }
		  }
	       }
	    }
	 }
      }
   }
   return ao;
}


// for all-atom contacts
bool
coot::atom_overlaps_container_t::is_inside_another_atom_to_which_its_bonded(int atom_idx,
									    mmdb::Atom *at,
									    const clipper::Coord_orth &pt_at_surface,
									    const std::vector<int> &bonded_neighb_indices,
									    mmdb::Atom **atom_selection) const {

   bool r = false;
   double r_1 = get_vdw_radius_neighb_atom(atom_idx);

   for (unsigned int i=0; i<bonded_neighb_indices.size(); i++) {
      mmdb::Atom *clash_neighb = atom_selection[bonded_neighb_indices[i]];
      clipper::Coord_orth pt_clash_neigh = co(clash_neighb);
      double r_2 = get_vdw_radius_neighb_atom(bonded_neighb_indices[i]);
      double r_2_sqrd = r_2 * r_2;
      double d_sqrd = (pt_at_surface - pt_clash_neigh).lengthsq();
//       std::cout << "debug this_atom " << atom_spec_t(at) << " has r_1 " << r_1
// 		<< " clash atom " << atom_spec_t(clash_neighb) << " has r_2 " << r_2 << std::endl;
      if (d_sqrd < r_2_sqrd) {
	 r = true;
	 break;
      }
   }

   return r;
}



// return H-bond, or wide-contact or close-contact or small-overlap or big-overlap or clash
//
std::string
coot::atom_overlaps_container_t::overlap_delta_to_contact_type(double delta, bool is_h_bond) const {

// from the Word 1999 paper:
// pale green for H-bonds
// green (narrow gaps) or yellow (slight overlaps, < 0.2) for good contacts
// blue for wider gaps > 0.25
// orange and red for unfavourable (0.25 to 0.4)
// hot pink for >= 0.4

   std::string r = "wide-contact";

   // std::cout << "overlap-delta " << delta << " " << is_h_bond << std::endl;

   if (is_h_bond) {
      if (delta >= 0) {
	 delta -= 0.7;
	 if (delta > 0.4)
	    r = "clash";
	 else
	    r = "H-bond";
      }
   } else {
      if (delta > -0.1)         // Word: -0.25, // -0.15 allows too much green
	 r = "close-contact";
      if (delta > 0.07)         // Word: 0
	 r = "small-overlap";
      if (delta > 0.33)         // Word: 0.2  // 0.15, 0.18, 0.2, 0.25, 0.3 allows too much red
	 r = "big-overlap";
      if (delta > 0.4)          // Word: 0.4 // 0.3, 0.35 too much clash
	 r = "clash";
   }
   return r;
}

void
coot::atom_overlaps_container_t::add_residue_neighbour_index_to_neighbour_atoms() {

   udd_residue_index_handle = mol->RegisterUDInteger(mmdb::UDR_ATOM, "neighb-residue-index");
   for (unsigned int i=0; i<neighbours.size(); i++) {
      mmdb::Atom **residue_atoms = 0;
      int n_residue_atoms;
      neighbours[i]->GetAtomTable(residue_atoms, n_residue_atoms);
      for (int iat=0; iat<n_residue_atoms; iat++) {
	 mmdb::Atom *at = residue_atoms[iat];
	 at->PutUDData(udd_residue_index_handle, int(i));
      }
   }
}



// fill std::vector<double> env_residue_radii;
//
// in the case of all-atom env_residues is all atoms
void
coot::atom_overlaps_container_t::setup_env_residue_atoms_radii(int i_sel_hnd_env_atoms) {

   double r = 1.5;
   mmdb::Atom **env_residue_atoms = 0;
   int n_env_residue_atoms;
   mol->GetSelIndex(i_sel_hnd_env_atoms, env_residue_atoms, n_env_residue_atoms);
   neighb_atom_radius.resize(n_env_residue_atoms);

   for (int i=0; i<n_env_residue_atoms; i++) {
      mmdb::Atom *at = env_residue_atoms[i];
      mmdb::Residue *res = at->residue;
      int residue_index;
      at->GetUDData(udd_residue_index_handle, residue_index);
      // const dictionary_residue_restraints_t &rest = neighb_dictionaries[residue_index];
      const dictionary_residue_restraints_t &rest = get_dictionary(res, residue_index);
      if (false) // debugging residue indexing
	 std::cout << "residue name " << res->GetResName() << " with comp_id "
		   << rest.residue_info.comp_id << std::endl;
      std::string te = rest.type_energy(at->GetAtomName());
      std::map<std::string, double>::const_iterator it_type = type_to_vdw_radius_map.find(te);
      if (it_type == type_to_vdw_radius_map.end()) {
	 // didn't find te in types map. so look it up from the dictionary and add to the types map
	 r = geom_p->get_energy_lib_atom(te).vdw_radius;
	 hb_t t = geom_p->get_energy_lib_atom(te).hb_type;
	 type_to_vdw_radius_map[te] = r;
      } else {
	 r = it_type->second;
      }
      neighb_atom_radius[i] = r;
   }
}

#include "geometry/main-chain.hh"

// We need here a flag for main-chain -> mainchain interactions also.
// Currently only consider side-chain -> side-chain and side-chain -> main-chain.
//
// Needs clear thinking.
//
// If atoms are bonded/1-3/ring related (or (note to self) possibly also 1-4?) then the atoms
// cannot clash and only these atoms can be neighbouring atoms that are tested to see if
// surface points are within the sphere of other atoms.
//
// so, either clashable or bonded or ignored
// 
coot::atom_overlaps_container_t::atom_interaction_type
coot::atom_overlaps_container_t::bonded_angle_or_ring_related(mmdb::Manager *mol,
							      mmdb::Atom *at_1,
							      mmdb::Atom *at_2,
							      bool exclude_mainchain_also,
							      std::map<std::string, std::vector<std::pair<std::string, std::string> > > &bonded_neighbours,
							      std::map<std::string, std::vector<std::vector<std::string> > > &ring_list_map) {

   // At the N terminus, the N has attached Hydrogen atoms H1, H2, H3.  These should not clash
   // with the N Nitrogen atom.

   // atom_spec_t debug_spec("A", 383, "", " OG ", "");
   atom_interaction_type ait = CLASHABLE;
   mmdb::Residue *res_1 = at_1->GetResidue();
   mmdb::Residue *res_2 = at_2->GetResidue();

   if (res_1 != res_2) {
      if (are_bonded_residues(res_1, res_2)) {
	 if (is_main_chain_p(at_1)) {
	    if (is_main_chain_p(at_2)) {
	       ait = BONDED; // :-) everything mainchain is bonded to each other (for dot masking)
	    } else {
	       // this is a bit hacky - don't allow clashes to CDs in proline from mainchain atoms
	       // (in other residues)
	       std::string res_name_2 = res_2->GetResName();
	       if (res_name_2 == "PRO") {
		  std::string at_name_1 = at_1->GetAtomName();
		  std::string at_name_2 = at_2->GetAtomName();
		  if (at_name_2 == " CD ") {
		     ait = BONDED;
		  } else {
		     ait = CLASHABLE;
		  }
	       } else {
		  ait = CLASHABLE;
	       }
	    }
	 } else {

	    // ---- at_1 is not main-chain
	    if (is_main_chain_p(at_2)) {
	       // perhaps the PRO CD - C atoms were the other way around (to above)
	       std::string at_name_1 = at_1->GetAtomName();
	       if (at_name_1 == " CD ") {
		  std::string res_name_2 = res_2->GetResName();
		  if (res_name_2 == "PRO") {
		     ait = BONDED;
		  } else {
		     ait = CLASHABLE;
		  }
	       } else {
		  ait = CLASHABLE;
	       }
	    } else {
	       ait = CLASHABLE;
	    }
	 }
      } else {
	 std::string res_name_1 = res_1->GetResName();
	 std::string res_name_2 = res_2->GetResName();
	 if (res_name_1 == res_name_2) {
	    if (res_name_1 == "HOH")
	       ait = IGNORED;
	 } else {
	    ait = CLASHABLE;
	 }
      }
   } else {

      // same residue
      //
      std::string res_name = res_1->GetResName();
      std::vector<std::pair<std::string, std::string> > bps =
	 geom_p->get_bonded_and_1_3_angles(res_name, protein_geometry::IMOL_ENC_ANY);
      std::string atom_name_1 = at_1->name;
      std::string atom_name_2 = at_2->name;
      std::map<std::string, std::vector<std::pair<std::string, std::string> > >::const_iterator it;
      it = bonded_neighbours.find(res_name);
      if (it == bonded_neighbours.end()) {
	 bps = geom_p->get_bonded_and_1_3_angles(res_name, protein_geometry::IMOL_ENC_ANY);
	 // std::cout << "adding " << bps.size() << " bonded pairs for type " << res_name << std::endl;
	 bonded_neighbours[res_name] = bps;
      } else {
	 bps = it->second;
      }
      for (unsigned int ipr=0; ipr<bps.size(); ipr++) {

	 if (atom_name_1 == bps[ipr].first) {
	    if (atom_name_2 == bps[ipr].second) {
	       ait = BONDED;
	       break;
	    }
	 }
	 if (atom_name_2 == bps[ipr].first) {
	    if (atom_name_1 == bps[ipr].second) {
	       ait = BONDED;
	       break;
	    }
	 }
      }
      if (ait == CLASHABLE) { // i.e. so far, unset by this block
	 bool ringed = in_same_ring(at_1, at_2, ring_list_map); // update ring_list_map
	 if (ringed) ait = BONDED;
      }
   }

   if (ait == CLASHABLE) {
      // maybe it was a link
      int imod = 1;
      mmdb::Model *model_p = mol->GetModel(imod);
      int n_links = model_p->GetNumberOfLinks();
      if (n_links > 0) {
	 for (int i_link=1; i_link<=n_links; i_link++) {
	    mmdb::PLink link = model_p->GetLink(i_link);
	    std::pair<atom_spec_t, atom_spec_t> atoms = link_atoms(link);
	    atom_spec_t spec_1(at_1);
	    atom_spec_t spec_2(at_2);
	    if (spec_1 == atoms.first)
	       if (spec_2 == atoms.second)
		  ait = BONDED;
	    if (spec_2 == atoms.first)
	       if (spec_1 == atoms.second)
		  ait = BONDED;
	 }
      }
   }

   return ait;
}

bool
coot::atom_overlaps_container_t::in_same_ring(mmdb::Atom *at_1, mmdb::Atom *at_2,
					      std::map<std::string, std::vector<std::vector<std::string> > > &ring_list_map) const {

   bool same = false;
   mmdb::Residue *res_1 = at_1->GetResidue();
   mmdb::Residue *res_2 = at_2->GetResidue();
   if (res_1 == res_2) {
      std::string at_name_1 = at_1->GetAtomName();
      std::string at_name_2 = at_2->GetAtomName();
      const dictionary_residue_restraints_t &dict = get_dictionary(res_1, 0);
      std::string res_name = at_1->GetResName();

      // old "calculate ring list each time" way
      // same = dict.in_same_ring(at_name_1, at_name_2);

      std::map<std::string, std::vector<std::vector<std::string> > >::const_iterator it;
      it = ring_list_map.find(res_name);
      if (it != ring_list_map.end()) {
	 // same = in_same_ring(at_name_1, at_name_2, it->second);
	 same = dict.in_same_ring(at_name_1, at_name_2, it->second);
      } else {
	 // on reflection, 5-membered rings are probably not needed because in those cases all atoms are
	 // bonded or 1-3 angles
	 std::vector<std::vector<std::string> > ring_list;
	 if (res_name == "HIS") {
	    ring_list = his_ring_list();
	 } else {
	    if (res_name == "PHE" || res_name == "TYR") {
	       ring_list = phe_ring_list();
	    } else {
	       if (res_name == "TRP") {
		  ring_list = trp_ring_list();
	       } else {
		  if (res_name == "PRO") {
		     ring_list = pro_ring_list();
		  } else {
		     ring_list = dict.get_ligand_ring_list();
		  }
	       }
	    }
	 }
	 ring_list_map[res_name] = ring_list;
	 // same = in_same_ring(at_name_1, at_name_2, ring_list);
	 same = dict.in_same_ring(at_name_1, at_name_2, ring_list);
      }
   }
   return same;
}

// moved to dictionary_residue_restraints_t
//
// bool
// coot::atom_overlaps_container_t::in_same_ring(const std::string &atom_name_1,
// 					      const std::string &atom_name_2,
// 					      const std::vector<std::vector<std::string> > &ring_list) const {

//    bool match = false;
//    for (unsigned int i=0; i<ring_list.size(); i++) {
//       unsigned int n_match = 0;
//       for (unsigned int j=0; j<ring_list[i].size(); j++) {
// 	 if (ring_list[i][j] == atom_name_1)
// 	    n_match++;
// 	 if (ring_list[i][j] == atom_name_2)
// 	    n_match++;
//       }
//       if (n_match == 2) {
// 	 match = true;
// 	 break;
//       }
//    }
//    return match;
// }

std::vector<std::vector<std::string> >
coot::atom_overlaps_container_t::phe_ring_list() const {
   std::vector<std::vector<std::string> > v;
   std::vector<std::string> vi(6);
   vi[0] = " CG ";
   vi[1] = " CD1";
   vi[2] = " CD2";
   vi[3] = " CE1";
   vi[4] = " CE2";
   vi[5] = " CZ ";
   v.push_back(vi);
   return v;
}


std::vector<std::vector<std::string> >
coot::atom_overlaps_container_t::his_ring_list() const {
   std::vector<std::vector<std::string> > v;
   std::vector<std::string> vi(5);
   vi[0] = " CG ";
   vi[1] = " ND1";
   vi[2] = " CD2";
   vi[3] = " NE2";
   vi[4] = " CE1";
   v.push_back(vi);
   return v;
}

std::vector<std::vector<std::string> >
coot::atom_overlaps_container_t::trp_ring_list() const {

   std::vector<std::vector<std::string> > v;
   std::vector<std::string> vi(5);
   std::vector<std::string> vi2(6);
   vi[0] = " CG ";
   vi[1] = " CD1";
   vi[2] = " NE1";
   vi[3] = " CE2";
   vi[4] = " CD2";
   vi2[0] = " CE2";
   vi2[1] = " CD2";
   vi2[2] = " CE3";
   vi2[3] = " CZ3";
   vi2[4] = " CH2";
   vi2[5] = " CZ2";
   v.push_back(vi);
   v.push_back(vi2);
   return v;
}

std::vector<std::vector<std::string> >
coot::atom_overlaps_container_t::pro_ring_list() const {

   std::vector<std::vector<std::string> > v;
   std::vector<std::string> vi(5);
   vi[0] = " CA ";
   vi[1] = " CB";
   vi[2] = " CG";
   vi[3] = " CD";
   vi[4] = " N";
   v.push_back(vi);
   return v;
}


bool
coot::atom_overlaps_container_t::are_bonded_residues(mmdb::Residue *res_1, mmdb::Residue *res_2) const {

   bool r = false;

   if (res_1) {
      if (res_2) {
	 if (res_1->GetChain() == res_2->GetChain()) {

	    // simple

	    if (abs(res_1->GetSeqNum() - res_2->GetSeqNum()) < 2) {
	       std::string res_name_1 = res_1->GetResName();
	       std::string res_name_2 = res_2->GetResName();
	       if (res_name_1 != "HOH")
		  if (res_name_2 != "HOH")
		     r = true;
	    }
	 }
      }
   }
   return r;
}
