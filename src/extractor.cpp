#include <assert.h>
#include <list>
#include <set>
#include "extractor.h"
#include "utils.h"

VortexExtractor::VortexExtractor(const Parallel::Communicator &comm)
  : ParallelObject(comm), 
    _verbose(0), 
    _mesh(NULL), 
    _exio(NULL), 
    _eqsys(NULL), 
    _tsys(NULL),
    _gauge(false)
{
}

VortexExtractor::~VortexExtractor()
{
  if (_eqsys) delete _eqsys; 
  if (_exio) delete _exio; 
  if (_mesh) delete _mesh; 
}

void VortexExtractor::SetVerbose(int level)
{
  _verbose = level; 
}

void VortexExtractor::SetMagneticField(const double B[3])
{
  memcpy(_B, B, sizeof(double)*3); 
}

void VortexExtractor::SetKex(double Kex)
{
  _Kex = Kex; 
}

void VortexExtractor::SetGaugeTransformation(bool g)
{
  _gauge = g; 
}

void VortexExtractor::LoadData(const std::string& filename)
{
  /// mesh
  _mesh = new Mesh(comm()); 
  _exio = new ExodusII_IO(*_mesh);
  _exio->read(filename);
  _mesh->allow_renumbering(false); 
  _mesh->prepare_for_use();

  if (Verbose())
    _mesh->print_info(); 

  /// equation systems
  _eqsys = new EquationSystems(*_mesh); 
  
  _tsys = &(_eqsys->add_system<NonlinearImplicitSystem>("GLsys"));
  _u_var = _tsys->add_variable("u", FIRST, LAGRANGE);
  _v_var = _tsys->add_variable("v", FIRST, LAGRANGE); 

  _eqsys->init(); 
  
  if (Verbose())
    _eqsys->print_info();
}

void VortexExtractor::LoadTimestep(int timestep)
{
  assert(_exio != NULL); 

  _timestep = timestep;

  if (Verbose())
    fprintf(stderr, "copying nodal solution... timestep=%d\n", timestep); 

  /// copy nodal data
  _exio->copy_nodal_solution(*_tsys, "u", "u", timestep); 
  _exio->copy_nodal_solution(*_tsys, "v", "v", timestep);
  
  if (Verbose())
    fprintf(stderr, "nodal solution copied, timestep=%d\n", timestep); 
}

void VortexExtractor::Extract()
{
  if (Verbose())
    fprintf(stderr, "extracting singularities on mesh faces...\n"); 
  
  const DofMap &dof_map  = _tsys->get_dof_map();  
  MeshBase::const_element_iterator it = _mesh->active_local_elements_begin(); 
  const MeshBase::const_element_iterator end = _mesh->active_local_elements_end(); 
 
  std::vector<float> zeros; 

  for (; it!=end; it++) {
    const Elem *elem = *it;
    VortexItem<> item; 
    
    for (int face=0; face<elem->n_sides(); face++) {
      AutoPtr<Elem> side = elem->side(face); 
      
      std::vector<dof_id_type> u_di, v_di; 
      dof_map.dof_indices(side.get(), u_di, _u_var);
      dof_map.dof_indices(side.get(), v_di, _v_var);
     
      // could use solution->get()
      double u[3] = {(*_tsys->solution)(u_di[0]), (*_tsys->solution)(u_di[1]), (*_tsys->solution)(u_di[2])}, 
             v[3] = {(*_tsys->solution)(v_di[0]), (*_tsys->solution)(v_di[1]), (*_tsys->solution)(v_di[2])};

      Node *nodes[3] = {side->get_node(0), side->get_node(1), side->get_node(2)};
      double X0[3] = {nodes[0]->slice(0), nodes[0]->slice(1), nodes[0]->slice(2)}, 
             X1[3] = {nodes[1]->slice(0), nodes[1]->slice(1), nodes[1]->slice(2)}, 
             X2[3] = {nodes[2]->slice(0), nodes[2]->slice(1), nodes[2]->slice(2)};
      double rho[3] = {sqrt(u[0]*u[0]+v[0]*v[0]), sqrt(u[1]*u[1]+v[1]*v[1]), sqrt(u[2]*u[2]+v[2]*v[2])}, 
             phi[3] = {atan2(v[0], u[0]), atan2(v[1], u[1]), atan2(v[2], u[2])}; 

      // check phase shift
      double flux = 0.f; // TODO: need to compute the flux correctly
      double delta[3];

      if (_gauge) {
        delta[0] = phi[1] - phi[0] - gauge_transformation(X0, X1, _Kex, _B); 
        delta[1] = phi[2] - phi[1] - gauge_transformation(X1, X2, _Kex, _B); 
        delta[2] = phi[0] - phi[2] - gauge_transformation(X2, X0, _Kex, _B); 
      } else {
        delta[0] = phi[1] - phi[0]; 
        delta[1] = phi[2] - phi[1]; 
        delta[2] = phi[0] - phi[2]; 
      }

      double delta1[3];  
      double phase_shift = 0.f; 
      for (int k=0; k<3; k++) {
        delta1[k] = mod2pi(delta[k] + M_PI) - M_PI; 
        phase_shift += delta1[k]; 
      }
      phase_shift += flux; 
      double critera = phase_shift / (2*M_PI);
      if (fabs(critera)<0.5f) continue; // not punctured
     
      // update bits
      int chirality = lround(critera);
      item.elem_id = elem->id(); 
      item.SetChirality(face, chirality);
      item.SetPuncturedFace(face);

      if (_gauge) {
        phi[1] = phi[0] + delta1[0]; 
        phi[2] = phi[1] + delta1[1];
        u[1] = rho[1] * cos(phi[1]);
        v[1] = rho[1] * sin(phi[1]); 
        u[2] = rho[2] * cos(phi[2]); 
        v[2] = rho[2] * sin(phi[2]);
      }

      double pos[3]; 
      bool succ = find_zero_triangle(u, v, X0, X1, X2, pos); 
      if (succ) {
        item.SetPuncturedPoint(face, pos); 

        zeros.push_back(pos[0]); 
        zeros.push_back(pos[1]); 
        zeros.push_back(pos[2]);
      } else {
        fprintf(stderr, "WARNING: punctured but singularities not found\n"); 
      }
    }
  
    if (item.Valid()) {
      _map[elem->id()] = item; 
      // fprintf(stderr, "elem_id=%d, bits=%s\n", 
      //     elem->id(), item.bits.to_string().c_str()); 
    }
  }
 
#if 0
  int npts = zeros.size()/3; 
  fprintf(stderr, "total number of singularities: %d\n", npts); 
  FILE *fp = fopen("out", "wb");
  fwrite(&npts, sizeof(int), 1, fp); 
  fwrite(zeros.data(), sizeof(float), zeros.size(), fp); 
  fclose(fp);
#endif
}

void VortexExtractor::Trace()
{
  std::vector<VortexObject<> > vortex_objects; 

  while (!_map.empty()) {
    /// 1. sort vortex items into connected components; pick up special items
    std::list<VortexMap<>::iterator> to_erase, to_visit;
    to_visit.push_back(_map.begin()); 

    VortexMap<> ordinary_items, special_items; 
    while (!to_visit.empty()) { // depth-first search
      VortexMap<>::iterator it = to_visit.front();
      to_visit.pop_front();
      if (it->second.visited) continue; 

      Elem *elem = _mesh->elem(it->first); 
      for (int face=0; face<4; face++) { // for 4 faces, in either directions
        Elem *neighbor = elem->neighbor(face); 
        if (it->second.IsPunctured(face) && neighbor != NULL) {
          VortexMap<>::iterator it1 = _map.find(neighbor->id());
          assert(it1 != _map.end());
          if (!it1->second.visited)
            to_visit.push_back(it1); 
        }
      }

      if (it->second.IsSpecial()) 
        special_items[it->first] = it->second;
      else 
        ordinary_items[it->first] = it->second; 

      it->second.visited = true; 
      to_erase.push_back(it);
    }
   
    for (std::list<VortexMap<>::iterator>::iterator it = to_erase.begin(); it != to_erase.end(); it ++)
      _map.erase(*it);
    to_erase.clear(); 
   
#if 1
    /// 2. trace vortex lines
    VortexObject<> vortex_object; 
    //// 2.1 special items
    for (VortexMap<>::iterator it = special_items.begin(); it != special_items.end(); it ++) {
      std::list<double> line;
      Elem *elem = _mesh->elem(it->first);
      Point centroid = elem->centroid(); 
      line.push_back(centroid(0)); line.push_back(centroid(1)); line.push_back(centroid(2));
      vortex_object.push_back(line); 
    }
    if (vortex_object.size() > 0)
      fprintf(stderr, "# of SPECIAL vortex items: %lu\n", vortex_object.size()); 

    //// 2.2 ordinary items
    for (VortexMap<>::iterator it = ordinary_items.begin(); it != ordinary_items.end(); it ++) 
      it->second.visited = false; 
    while (!ordinary_items.empty()) {
      VortexMap<>::iterator seed = ordinary_items.begin(); 
      bool special; 
      std::list<double> line; 
      to_erase.push_back(seed);

      // trace forward (chirality = 1)
      ElemIdType id = seed->first;       
      while (1) {
        int face; 
        double pos[3];
        bool traced = false; 
        Elem *elem = _mesh->elem(id); 
        VortexMap<>::iterator it = ordinary_items.find(id);
        if (it == ordinary_items.end()) {
          special = true;
          it = special_items.find(id);
        } else {
          special = false;
          // if (it->second.visited) break;  // avoid loop
          // else it->second.visited = true; 
        }
        
        for (face=0; face<4; face++) 
          if (it->second.Chirality(face) == 1) {
            if (it != seed) 
              to_erase.push_back(it); 
            it->second.GetPuncturedPoint(face, pos);
            line.push_back(pos[0]); line.push_back(pos[1]); line.push_back(pos[2]);
            Elem *neighbor = elem->neighbor(face);
            if (neighbor != NULL) {
              id = neighbor->id(); 
              if (special)  // `downgrade' the special element
                it->second.RemovePuncturedFace(face); 
              traced = true; 
            }
          }

        if (!traced) break;
      }

      // trace backward (chirality = -1)
      line.pop_front(); line.pop_front(); line.pop_front(); // remove the seed point
      id = seed->first;       
      while (1) {
        int face; 
        double pos[3];
        bool traced = false; 
        Elem *elem = _mesh->elem(id); 
        VortexMap<>::iterator it = ordinary_items.find(id);
        if (it == ordinary_items.end()) {
          special = true;
          it = special_items.find(id);
        } else {
          special = false;
          // if (it->second.visited) break; // avoid loop
          // else it->second.visited = true; 
        }
        
        for (face=0; face<4; face++) 
          if (it->second.Chirality(face) == -1) {
            if (it != seed) 
              to_erase.push_back(it); 
            it->second.GetPuncturedPoint(face, pos);
            line.push_front(pos[0]); line.push_front(pos[1]); line.push_front(pos[2]); 
            Elem *neighbor = elem->neighbor(face);
            if (neighbor != NULL) {
              id = neighbor->id(); 
              if (special)  // `downgrade' the special element
                it->second.RemovePuncturedFace(face); 
              traced = true; 
            }
          }
        if (!traced) break;
      }
      
      for (std::list<VortexMap<>::iterator>::iterator it = to_erase.begin(); it != to_erase.end(); it ++)
        ordinary_items.erase(*it);
      to_erase.clear();

      vortex_object.push_back(line); 
    }

    vortex_objects.push_back(vortex_object); 

    fprintf(stderr, "# of lines in vortex_object: %lu\n", vortex_object.size());
    int count = 0; 
    for (VortexObject<>::iterator it = vortex_object.begin(); it != vortex_object.end(); it ++) {
      fprintf(stderr, " - line %d, # of vertices: %lu\n", count ++, it->size()/3); 
    }
#endif
  }
    
  fprintf(stderr, "# of vortex objects: %lu\n", vortex_objects.size());

  size_t offset_size[2] = {0}; 
  FILE *fp_offset = fopen("offset", "wb"), 
       *fp = fopen("vortex", "wb");
  size_t count = vortex_objects.size(); 
  fwrite(&count, sizeof(size_t), 1, fp_offset); 
  for (int i=0; i<vortex_objects.size(); i++) {
    std::string buf; 
    vortex_objects[i].Serialize(buf);
    offset_size[1] = buf.size();

    fwrite(offset_size, sizeof(size_t), 2, fp_offset);
    fwrite(buf.data(), 1, buf.size(), fp); 

    fprintf(stderr, "offset=%lu, size=%lu\n", offset_size[0], offset_size[1]); 

    offset_size[0] += buf.size(); 
  }
}
