#ifndef _VORTEX_TRANSITION_H
#define _VORTEX_TRANSITION_H

#include "def.h"
#include "common/VortexTransitionMatrix.h"
#include "common/VortexSequence.h"
#include <utility>
#include <mutex>

#if WITH_ROCKSDB
#include <rocksdb/db.h>
#endif

class VortexTransition 
{
  friend class diy::Serialization<VortexTransition>;
public:
  VortexTransition();
  ~VortexTransition();
  
  // int ts() const {return _ts;}
  // int tl() const {return _tl;}

#ifdef WITH_ROCKSDB
  bool LoadFromDB(rocksdb::DB*);
#endif

  void LoadFromFile(const std::string &dataname, int ts, int tl);
  void SaveToDotFile(const std::string &filename) const;

  VortexTransitionMatrix& Matrix(Interval intervals);
  void AddMatrix(const VortexTransitionMatrix& m);
  int Transition(int t, int i, int j) const;
  // const std::map<int, VortexTransitionMatrix>& Matrices() const {return _matrices;}
  std::map<Interval, VortexTransitionMatrix>& Matrices() {return _matrices;}

  void ConstructSequence();
  void PrintSequence() const;
  void SequenceGraphColoring();
  void SequenceColor(int gid, unsigned char &r, unsigned char &g, unsigned char &b) const;

  int lvid2gvid(int frame, int lvid) const;
  int gvid2lvid(int frame, int gvid) const;

  int MaxNVorticesPerFrame() const {return _max_nvortices_per_frame;}
  int NVortices(int frame) const;

  const std::vector<struct VortexSequence> Sequences() const {return _seqs;}
  void RandomColorSchemes();
  
  const std::vector<struct VortexEvent>& Events() const {return _events;}

  int TimestepToFrame(int timestep) const {return _frames[timestep];} // confusing.  TODO: change func name
  int Frame(int i) const {return _frames[i];}
  int NTimesteps() const {return _frames.size();}
  void SetFrames(const std::vector<int> frames) {_frames = frames;}
  const std::vector<int>& Frames() const {return _frames;}

private:
  int NewVortexSequence(int its);
  std::string NodeToString(int i, int j) const;

private:
  // int _ts, _tl;
  std::vector<int> _frames; // frame IDs
  std::map<Interval, VortexTransitionMatrix> _matrices;
  std::vector<struct VortexSequence> _seqs;
  std::map<std::pair<int, int>, int> _seqmap; // <time, lid>, gid
  std::map<std::pair<int, int>, int> _invseqmap; // <time, gid>, lid
  std::map<int, int> _nvortices_per_frame;
  int _max_nvortices_per_frame;

  std::vector<struct VortexEvent> _events;

  std::mutex _mutex;
};

/////////
namespace diy {
  template <> struct Serialization<VortexTransition> {
    static void save(diy::BinaryBuffer& bb, const VortexTransition& m) {
      diy::save(bb, m._frames);
      diy::save(bb, m._matrices);
      diy::save(bb, m._seqs);
      diy::save(bb, m._seqmap);
      diy::save(bb, m._invseqmap);
      diy::save(bb, m._nvortices_per_frame);
      diy::save(bb, m._max_nvortices_per_frame);
      diy::save(bb, m._events);
    }

    static void load(diy::BinaryBuffer&bb, VortexTransition& m) {
      diy::load(bb, m._frames);
      diy::load(bb, m._matrices);
      diy::load(bb, m._seqs);
      diy::load(bb, m._seqmap);
      diy::load(bb, m._invseqmap);
      diy::load(bb, m._nvortices_per_frame);
      diy::load(bb, m._max_nvortices_per_frame);
      diy::load(bb, m._events);
    }
  };
}

#endif
