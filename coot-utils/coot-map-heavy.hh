/* coot-utils/coot-map-utils.hh
 * 
 * Copyright 2004, 2005, 2006 The University of York
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
 * 02110-1301, USA.
 */


#include "clipper/core/coords.h"
#include "clipper/core/xmap.h"
#include <mmdb2/mmdb_manager.h>
#ifdef HAVE_GSL
#include "gsl/gsl_multimin.h"
#endif // HAVE_GSL
#include "clipper/core/nxmap.h"
#include "mini-mol/mini-mol.hh"
#include "geometry/protein-geometry.hh"

namespace coot {

   namespace util { 

#ifdef HAVE_GSL

      class simplex_param_t {
      public:
	 mmdb::PPAtom orig_atoms;
	 clipper::Coord_orth atoms_centre;
	 int n_atoms;
	 const clipper::Xmap<float> *xmap;
      };
      
      // Use asc.atom_selection as the moving atoms
      // and move them.
      // 
      int fit_to_map_by_simplex_rigid(mmdb::PPAtom atom_selection,
				int n_selected_atoms,
				const clipper::Xmap<float> &xmap);

      // internal simplex setup function:
      void setup_simplex_x_internal_rigid(gsl_vector *s,
				    mmdb::PPAtom atom_selection,
				    int n_selected_atoms);

      // the function that returns the value:
      double my_f_simplex_rigid_internal (const gsl_vector *v,
				    void *params);

      void simplex_apply_shifts_rigid_internal(gsl_vector *s,
					 simplex_param_t &par);

      float z_weighted_density_at_point(const clipper::Coord_orth &pt,
					const std::string &ele,
					const std::vector<std::pair<std::string, int> > &atom_number_list,
					const clipper::Xmap<float> &map_in);

      float z_weighted_density_at_point_linear_interp(const clipper::Coord_orth &pt,
						      const std::string &ele,
						      const std::vector<std::pair<std::string, int> > &atom_number_list,
						      const clipper::Xmap<float> &map_in);

      float z_weighted_density_score(const std::vector<mmdb::Atom *> &atoms,
				     const std::vector<std::pair<std::string, int> > &atom_number_list,
				     const clipper::Xmap<float> &map);

      float z_weighted_density_score(const std::vector<mmdb::Atom *> &atoms,
				     const std::vector<std::pair<std::string, int> > &atom_number_list,
				     const clipper::Xmap<float> &map);

      float z_weighted_density_score_new(const std::vector<std::pair<mmdb::Atom *, float> > &atom_atom_number_pairs,
					 const clipper::Xmap<float> &map);

      float z_weighted_density_score(const minimol::molecule &mol,
				     const std::vector<std::pair<std::string, int> > &atom_number_list,
				     const clipper::Xmap<float> &map);

      float z_weighted_density_score_linear_interp(const minimol::molecule &mol,
						   const std::vector<std::pair<std::string, int> > &atom_number_list,
						   const clipper::Xmap<float> &map);

      float biased_z_weighted_density_score(const minimol::molecule &mol,
					    const std::vector<std::pair<std::string, int> > &atom_number_list,
					    const clipper::Xmap<float> &map);

      std::vector<std::pair<std::string, float> > score_atoms(const minimol::residue &residue_res,
							      const clipper::Xmap<float> &xmap);

      // if annealing_factor is > 0, then scale the offsets by this amount
      // (so, between 0 and 1)
      // 
      std::pair<clipper::RTop_orth, std::vector<mmdb::Atom> >
      jiggle_atoms(const std::vector<mmdb::Atom *> &atoms,
		   const clipper::Coord_orth &centre_pt,
		   float jiggle_scale_factor,
		   float annealing_factor=1.0);
      std::pair<clipper::RTop_orth, std::vector<mmdb::Atom> >
      jiggle_atoms(const std::vector<mmdb::Atom> &atoms,
		   const clipper::Coord_orth &centre_pt,
		   float jiggle_scale_factor,
		   float annealing_factor=1.0);
      clipper::RTop_orth make_rtop_orth_for_jiggle_atoms(float jiggle_trans_scale_factor,
							 float annealing_factor);


#endif // HAVE_GSL      

      class fffear_search {
	 int fill_nxmap     (mmdb::Manager *mol, int SelectionHandle,
			      const clipper::Coord_orth &low_left);
	 int fill_nxmap_mask(mmdb::Manager *mol, int SelectionHandle,
			      const clipper::Coord_orth &low_left);
	 void generate_search_rtops(float angular_resolution); // fill ops (e.g. 10 degrees)
	 double min_molecule_radius_; 

	 // For each grid point store a best score and the rtop that
	 // corresponds to it.
	 clipper::Xmap<std::pair<float, int> > results;
	 std::vector<clipper::RTop_orth> ops;
	 clipper::Coord_orth mid_point_; // of the search molecule.  Applied to the
			                 // molecule before the rtop of the fffear
  			                 // search.
	 void post_process_nxmap(float xmap_mean, float xmap_stddev);
	 // filter the peaks of the results map by distance.  We don't
	 // want smaller peaks that overlap the positions of higher
	 // peaks.  So there needs to be some sort of distance
	 // searching of the trns components of the rtops.  We use the
	 // molecule size (min_molecule_radius_) to do that.
	 std::vector<std::pair<float, clipper::RTop_orth> >
	 filter_by_distance_to_higher_peak(const std::vector<std::pair<float, clipper::RTop_orth> > &vr) const;
	 
      public:
	 clipper::NXmap<float> nxmap;
	 clipper::NXmap<float> nxmap_mask;
	 fffear_search(mmdb::Manager *mol, int SelectionHandle, const clipper::Xmap<float> &xmap,
		       float angular_resolution, bool translation_search_only=false);
	 std::vector<std::pair<clipper::RTop_orth, float> > get_scored_transformations() const;
	 clipper::RTop_orth get_best_transformation() const;
	 clipper::Xmap<float> get_results_map() const;
	 std::vector<std::pair<float, clipper::RTop_orth> > scored_orientations() const;
	 // transformations of the coords need to apply this
	 // translation of the molecule (to be centered round the
	 // origin) before applying the RTop of the (negative) peaks of the fffear map.
	 clipper::RTop_orth mid_point_transformation() const;
      };

   }
}
