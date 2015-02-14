#include "Extractor.h"
#include "common/Utils.hpp"
#include "io/GLDataset.h"
#include <set>
#include <cstdlib>
#include <cassert>

VortexExtractor::VortexExtractor() :
  _dataset(NULL), 
  _gauge(false)
{

}

VortexExtractor::~VortexExtractor()
{

}

void VortexExtractor::SetDataset(const GLDatasetBase* ds)
{
  _dataset = ds;
}

void VortexExtractor::SetGaugeTransformation(bool g)
{
  _gauge = g; 
}

void VortexExtractor::SaveVortexLines(const std::string& filename)
{
  ::SaveVortexLines(_vortex_lines, filename); 
}

void VortexExtractor::ClearPuncturedObjects()
{
  _punctured_edges.clear();
  _punctured_faces.clear();
  _punctured_faces1.clear();
}

bool VortexExtractor::SavePuncturedEdges() const
{
  const GLDatasetBase *ds = _dataset;
  std::ostringstream os; 
  os << ds->DataName() << ".pe." << ds->TimeStep() << "." << ds->TimeStep1();
  return ::SavePuncturedEdges(_punctured_edges, os.str());
}

bool VortexExtractor::SavePuncturedFaces(int time) const
{
  const GLDatasetBase *ds = _dataset;
  std::ostringstream os; 
  os << ds->DataName() << ".pf." << 
    (time == 0 ? ds->TimeStep() : ds->TimeStep1());
  return ::SavePuncturedFaces(_punctured_faces, os.str());
}

bool VortexExtractor::LoadPuncturedEdges()
{
  const GLDatasetBase *ds = _dataset;
  std::ostringstream os; 
  os << ds->DataName() << ".pe." << ds->TimeStep() << "." << ds->TimeStep1();
 
  std::map<EdgeIdType, PuncturedEdge> m;
  if (!::LoadPuncturedEdges(m, os.str())) return false;
  
  for (std::map<EdgeIdType, PuncturedEdge>::iterator it = m.begin(); it != m.end(); it ++) 
    AddPuncturedEdge(it->first, it->second.chirality, it->second.t);
  
  return true;
}

bool VortexExtractor::LoadPuncturedFaces(int time)
{
  const GLDatasetBase *ds = _dataset;
  std::ostringstream os; 
  os << ds->DataName() << ".pf." << 
    (time == 0 ? ds->TimeStep() : ds->TimeStep1());
  
  std::map<FaceIdType, PuncturedFace> m; 

  if (!::LoadPuncturedFaces(m, os.str())) return false;

  for (std::map<FaceIdType, PuncturedFace>::iterator it = m.begin(); it != m.end(); it ++) {
    AddPuncturedFace(it->first, time, it->second.chirality, it->second.pos);
  }

  return true;
}

void VortexExtractor::AddPuncturedFace(FaceIdType id, int time, ChiralityType chirality, const double pos[])
{
  // face
  PuncturedFace pf;

  pf.chirality = chirality;
  memcpy(pf.pos, pos, sizeof(double)*3);

  if (time == 0)
    _punctured_faces[id] = pf;
  else 
    _punctured_faces1[id] = pf;

  // vcell
  PuncturedCell &vc = _punctured_vcells[id];
  if (time == 0)
    vc.SetChirality(0, -chirality);
  else 
    vc.SetChirality(1, chirality);

  // cell
  const MeshGraph &mg = _dataset->MeshGraph();
  CFace face = mg.faces[id];
  for (int i=0; i<face.contained_cells.size(); i++) {
    CellIdType cid = face.contained_cells[i]; 
    int fchirality = face.contained_cells_chirality[i];
    int fid = face.contained_cells_fid[i];
    PuncturedCell &c = _punctured_cells[cid]; 
    c.SetChirality(fid, chirality * fchirality);
#if 0
    fprintf(stderr, "face=%u, cid=%u, fid=%d, chi=%d, fchi=%d\n", 
        id, cid, fid, chirality, fchirality);
#endif
  }
}

void VortexExtractor::AddPuncturedEdge(EdgeIdType id, ChiralityType chirality, double t)
{
  // edge
  PuncturedEdge pe;

  pe.chirality = chirality;
  pe.t = t;

  _punctured_edges[id] = pe;

  // vface
  const MeshGraph &mg = _dataset->MeshGraph();
  CEdge edge = mg.edges[id];
  for (int i=0; i<edge.contained_faces.size(); i++) {
    int echirality = edge.contained_faces_chirality[i];
    int eid = edge.contained_faces_eid[i]; 
    PuncturedCell &vc = _punctured_vcells[edge.contained_faces[i]];
    vc.SetChirality(eid+2, chirality * echirality);
  }
}
  
bool VortexExtractor::FindSpaceTimeEdgeZero(const double re[], const double im[], double &t) const
{
  double p[2];
  if (!find_zero_unit_quad_bilinear(re, im, p))
    return false;

  t = p[1];
  return true;
}

void VortexExtractor::TraceOverTime()
{
  fprintf(stderr, "Tracing over time, #pf0=%ld, #pf1=%ld, #pe=%ld\n", 
      _punctured_faces.size(), _punctured_faces1.size(), _punctured_edges.size());
  const MeshGraph &mg = _dataset->MeshGraph();

  for (std::map<FaceIdType, PuncturedFace>::iterator it = _punctured_faces.begin(); 
       it != _punctured_faces.end(); it ++) 
  {
    // fprintf(stderr, "fid=%u\n", it->first);

    std::vector<FaceIdType> related;
    
    std::list<FaceIdType> faces_to_visit;
    std::list<int> faces_to_visit_chirality; // face chirality
    std::list<double> faces_to_visit_time;
    std::set<FaceIdType> faces_visited;
    std::set<EdgeIdType> edges_visited;
    
    faces_to_visit.push_back(it->first);
    faces_to_visit_chirality.push_back(it->second.chirality);
    faces_to_visit_time.push_back(0);
      
    std::map<FaceIdType, FaceIdType> parent_map;
    std::map<FaceIdType, std::tuple<EdgeIdType, double> > parent_edge_map;

    while (!faces_to_visit.empty()) {
      FaceIdType current = faces_to_visit.front();
      int current_chirality = faces_to_visit_chirality.front();
      double current_time = faces_to_visit_time.front();

      faces_to_visit.pop_front();
      faces_to_visit_chirality.pop_front();
      faces_to_visit_time.pop_front();

      faces_visited.insert(current);

      if (_punctured_faces1.find(current) != _punctured_faces1.end() && 
          _punctured_faces1[current].chirality == current_chirality) 
      {
        related.push_back(current);

        std::list<FaceIdType> history_faces;
        std::list<std::tuple<EdgeIdType, double> > history_edges;

        history_faces.push_back(current);
        std::map<FaceIdType, FaceIdType>::iterator it = parent_map.find(current);
        while (it != parent_map.end()) {
          history_edges.push_front(parent_edge_map[it->first]);
          history_faces.push_front(it->second);
          it = parent_map.find(it->second);
        }

        if (history_faces.size() > 1) {
          int i=0;
          std::list<std::tuple<EdgeIdType, double> >::iterator it1 = history_edges.begin();
          for (std::list<FaceIdType>::iterator it = history_faces.begin(); 
               it != history_faces.end(); it ++) 
          {
            if (i<history_faces.size()-1) 
              fprintf(stderr, "%u->(%u, %.2f)->", *it, std::get<0>(*it1), std::get<1>(*it1));
            else 
              fprintf(stderr, "%u\n", *it);
            i++;
            it1++;
          }
        }
      }


      // add neighbors
      const CFace &face = mg.faces[current];
      for (int i=0; i<face.edges.size(); i++) {
        // find punctured edges
        EdgeIdType e = face.edges[i];
        if (_punctured_edges.find(e) != _punctured_edges.end() && 
            edges_visited.find(e) == edges_visited.end())
        {
          edges_visited.insert(e);
          
          const CEdge &edge = mg.edges[e];
          const PuncturedEdge& pe = _punctured_edges[e];
          if (current_time >= pe.t) continue; // time ascending order
          
          int echirality = face.edges_chirality[i] * pe.chirality;
          if (current_chirality == echirality) {
            /// find neighbor faces who chontain this edge
            // fprintf(stderr, "--fid=%u, found edge eid=%u, t=%f\n", current, e, pe.t);
            for (int j=0; j<edge.contained_faces.size(); j++) {
              if (faces_visited.find(edge.contained_faces[j]) == faces_visited.end()) { // not found in visited faces
                faces_to_visit.push_front(edge.contained_faces[j]);
                faces_to_visit_chirality.push_front(edge.contained_faces_chirality[j] * current_chirality);
                faces_to_visit_time.push_front(pe.t);
                parent_map[edge.contained_faces[j]] = current;
                parent_edge_map[edge.contained_faces[j]] = std::make_tuple(e, pe.t);
              }
            }
          }
        }
      }
    }

#if 0
    // non-ordinary
    if (!(related.size() == 1 && it->first == related[0])) {
      fprintf(stderr, "fid=%u, related={", it->first);
      for (int i=0; i<related.size(); i++)
        if (i<related.size()-1)
          fprintf(stderr, "%u, ", related[i]);
        else 
          fprintf(stderr, "%u", related[i]);
      fprintf(stderr, "}\n");
    }
#endif
  }
}

void VortexExtractor::TraceVirtualCells()
{
  int n_self = 0, n_pure = 0, n_cross = 0, n_invalid = 0;
  for (std::map<FaceIdType, PuncturedCell>::iterator it = _punctured_vcells.begin(); it != _punctured_vcells.end(); it ++) 
  {
    int c[5];
    for (int i=0; i<5; i++) 
      c[i] = it->second.Chirality(i);
    
    bool punctured = c[0] || c[1] || c[2] || c[3] || c[4];
    bool pure = punctured && !c[0] && !c[1];
    bool self = c[0] && c[1];
    bool cross = (c[0] || c[1]) && (c[2] || c[3] || c[4]);
    int sum = c[0] + c[1] + c[2] + c[3] + c[4];

    if (sum != 0) n_invalid ++;
    if (pure) n_pure ++;
    if (self) n_self ++;
    if (cross) n_cross ++;

    if (sum != 0) 
      fprintf(stderr, "--SPECIAL:\n");
    fprintf(stderr, "%d\t%d\t%d\t%d\t%d\n", c[0], c[1], c[2], c[3], c[4]);
  }
  fprintf(stderr, "n_self=%d, n_pure=%d, n_cross=%d, n_invalid=%d\n", 
      n_self, n_pure, n_cross, n_invalid);
}

void VortexExtractor::TraceOverSpace()
{
  fprintf(stderr, "tracing over space, #punctured_cells=%ld.\n", _punctured_cells.size());
  _vortex_lines.clear();

  std::map<CellIdType, PuncturedCell> pcs = _punctured_cells;
  const MeshGraph &mg = _dataset->MeshGraph();

  while (!pcs.empty()) {
    /// 1. sort punctured cells into connected ordinary/special ones
    std::list<CellIdType> to_visit;
    std::set<CellIdType> visited;
    
    to_visit.push_back(pcs.begin()->first); 
    std::map<CellIdType, PuncturedCell> ordinary_pcells, special_pcells;

    while (!to_visit.empty()) { // depth-first search
      CellIdType c = to_visit.front();
      to_visit.pop_front();
    
      const PuncturedCell &pcell = pcs[c];
      const CCell &cell = mg.cells[c];

      if (pcell.IsSpecial())
        special_pcells[c] = pcell;
      else 
        ordinary_pcells[c] = pcell;
      visited.insert(c);

      for (int i=0; i<cell.neighbor_cells.size(); i++) {
        CellIdType c1 = cell.neighbor_cells[i];
        if (c1 != UINT_MAX                            // valid neighbor cell
            && pcell.Chirality(i) != 0                // corresponding face punctured
            && pcs.find(c1) != pcs.end()              // neighbor cell punctured
            && visited.find(c1) == visited.end())     // not visited
        {
          to_visit.push_back(c1);
        }
      }
    }
  
    for (std::set<CellIdType>::iterator it = visited.begin(); it != visited.end(); it ++)
      pcs.erase(*it);
    visited.clear();

    fprintf(stderr, "#ordinary=%ld, #special=%ld\n", ordinary_pcells.size(), special_pcells.size());

#if 0
    /// 2. trace vortex lines
    VortexLine vortex_line; 
    /// 2.1 clear visited tags
    for (PuncturedElemMap::iterator it = ordinary_pelems.begin(); it != ordinary_pelems.end(); it ++) 
      it->second->visited = false;
    /// 2.2 trace backward and forward
    while (!ordinary_pelems.empty()) {
      PuncturedElemMap::iterator seed = ordinary_pelems.begin();
      to_erase.push_back(seed);

      std::list<double> line; 

      // trace forward (chirality == 1)
      ElemIdType id = seed->first;
      double pos[3];
      bool traced; 
      while (1) {
        traced = false;
        PuncturedElemMap::iterator it = ordinary_pelems.find(id);
        if (it == ordinary_pelems.end() || it->second->visited) break;
        it->second->visited = true;

        std::vector<ElemIdType> neighbors = _dataset->GetNeighborIds(it->first); 
        for (int face=0; face<neighbors.size(); face++) 
          if (it->second->Chirality(face) == 1) {
            if (it != seed) to_erase.push_back(it); 
            it->second->GetPuncturedPoint(face, pos);
            line.push_back(pos[0]); line.push_back(pos[1]); line.push_back(pos[2]);
            if (neighbors[face] != UINT_MAX && special_pelems.find(neighbors[face]) == special_pelems.end()) { // not boundary && not special
              id = neighbors[face]; 
              traced = true;
            }
          }

        if (_dataset->OnBoundary(id)) break;
        if (!traced) break;
      }

      // trace backward (chirality == -1)
      seed->second->visited = false;
      id = seed->first;
      while (1) {
        traced = false;
        PuncturedElemMap::iterator it = ordinary_pelems.find(id);
        if (it == ordinary_pelems.end() || it->second->visited) break;
        it->second->visited = true;

        std::vector<ElemIdType> neighbors = _dataset->GetNeighborIds(it->first); 
        for (int face=0; face<neighbors.size(); face++) 
          if (it->second->Chirality(face) == -1) {
            if (it != seed) to_erase.push_back(it); 
            it->second->GetPuncturedPoint(face, pos);
            line.push_front(pos[2]); line.push_front(pos[1]); line.push_front(pos[0]);
            if (neighbors[face] != UINT_MAX && special_pelems.find(neighbors[face]) == special_pelems.end()) { // not boundary && not special
              id = neighbors[face]; 
              traced = true;
            }
          }

        if (_dataset->OnBoundary(id)) break;
        if (!traced) break;
      }

      for (std::list<PuncturedElemMap::iterator>::iterator it = to_erase.begin(); it != to_erase.end(); it ++)
        ordinary_pelems.erase(*it);
      to_erase.clear();

      _vortex_lines.push_back(vortex_line);
    }
#endif 
  }
 
  // release memory
  // for (PuncturedElemMap::iterator it = punctured_elems1.begin(); it != punctured_elems1.end(); it ++)
  //   delete it->second;
  fprintf(stderr, "#vortex_lines=%ld\n", _vortex_lines.size());
}



#if 0
bool VortexExtractor::ExtractElem(ElemIdType id)
{
  const int nfaces = _dataset->NrFacesPerElem(); 
  PuncturedElem *pelem = NewPuncturedElem(id);
 
  for (int face=0; face<nfaces; face++) {
    const int nnodes = _dataset->NrNodesPerFace();
    double X[nnodes][3], A[nnodes][3], re[nnodes], im[nnodes]; 

    _dataset->GetFace(id, face, X, A, re, im);

    // compute rho & phi
    double rho[nnodes], phi[nnodes];
    for (int i=0; i<nnodes; i++) {
      rho[i] = sqrt(re[i]*re[i] + im[i]*im[i]);
      phi[i] = atan2(im[i], re[i]);
    }

    // calculating phase shift
    double delta[nnodes], phase_shift = 0;
    for (int i=0; i<nnodes; i++) {
      int j = (i+1) % nnodes;
      delta[i] = phi[j] - phi[i]; 
      if (_gauge) 
        delta[i] += _dataset->GaugeTransformation(X[i], X[j], A[i], A[j]) + _dataset->QP(X[i], X[j]); 
      delta[i] = mod2pi(delta[i] + M_PI) - M_PI;
      phase_shift -= delta[i];
      phase_shift -= _dataset->LineIntegral(X[i], X[j], A[i], A[j]);
    }

    // check if punctured
    double critera = phase_shift / (2*M_PI);
    if (fabs(critera)<0.5) continue; // not punctured

    // chirality
    int chirality = critera>0 ? 1 : -1;

    // gauge transformation
    if (_gauge) { 
      for (int i=1; i<nnodes; i++) {
        phi[i] = phi[i-1] + delta[i-1];
        re[i] = rho[i] * cos(phi[i]); 
        im[i] = rho[i] * sin(phi[i]);
      }
    }

    // find zero
    double pos[3];
    if (FindZero(X, re, im, pos))
      pelem->AddPuncturedFace(face, chirality, pos);
    else fprintf(stderr, "WARNING: punctured but singularity not found.\n");
  }

  if (pelem->Punctured()) {
    _punctured_elems[id] = pelem;
    if (pelem->Degree() != 2 && !_dataset->OnBoundary(id))
      fprintf(stderr, "degree=%d\n", pelem->Degree());
    return true;
  } else {
    delete pelem;
    return false;
  }
}
#endif
