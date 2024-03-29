
#ifndef PRSEEDINGXLAYERS_H 
#define PRSEEDINGYLAYERS_H 1

// Include files
// from Gaudi

//#define DEBUG_HISTO 
//uncomment this line if you want to print monitoring histograms

#ifdef DEBUG_HISTO 
#include "GaudiAlg/GaudiTupleAlg.h"
#else
#include "GaudiAlg/GaudiAlgorithm.h"
#endif
#include "GaudiAlg/ISequencerTimerTool.h"

#include "PrKernel/IPrDebugTool.h"
#include "PrKernel/PrHitManager.h"
#include "PrSeedTrack.h"
#include "PrGeometryTool.h"
#include "TfKernel/RecoFuncs.h"

/** @class PrSeedingXLayers PrSeedingXLayers.h
 *  Stand alone seeding for the FT T stations
 *  This code is a hack which represents the code used for the upgrade tracker TDR
 *  It needs to be superseded by a proper implementation!
 *
 * - InputName: Name of the input container for the forward tracks. Set to '""' to not reuse the FT part of the forward tracks.
 * - OutputName: Name of the output container
 * - HitManagerName: Name of the hit manager
 * - DecodeData: Decode the data (default: false, as normally done in the Forward Tracking)
 * - XOnly: Only reconstruct tracks with the x-layers?
 * - MaxChi2InTrack: Max Chi2 value in internal track fit.
 * - TolXInf: Lower bound for search "road"
 * - TolXSup: Upper bound for search "road"
 * - MinXPlanes: Minimum number of x-planes a track needs to have
 * - MaxChi2PerDoF: Maximum Chi2/nDoF a track can have.
 * - MaxParabolaSeedHits: Maximum number of hits which are use to construct a parabolic search window.
 * - TolTyOffset: Tolerance for the offset in y for adding stereo hits.
 * - TolTySlope: Tolerance for the slope in y for adding stereo hits.
 * - MaxIpAtZero: Maximum impact parameter of the track when doing a straight extrapolation to zero. Acts as a momentum cut.
 * - DebugToolName: Name of the debug tool
 * - WantedKey: Key of the particle which should be studied (for debugging).
 * - TimingMeasurement: Do timing measurement and print table at the end (?).
 * - PrintSettings: Print all values of the properties at the beginning?
 *
 *  @author Olivier Callot
 *  @date   2013-02-14
 *  2014-06-26 : Yasmine Amhis Modification
 *  2014-02-12 : Michel De Cian (TDR version) 
 */
#ifdef DEBUG_HISTO 
class PrSeedingXLayers : public GaudiTupleAlg {
#else 
class PrSeedingXLayers : public GaudiAlgorithm {
#endif
public:
  /// Standard constructor
  PrSeedingXLayers( const std::string& name, ISvcLocator* pSvcLocator );

  virtual ~PrSeedingXLayers( ); ///< Destructor

  virtual StatusCode initialize();    ///< Algorithm initialization
  virtual StatusCode execute   ();    ///< Algorithm execution
  virtual StatusCode finalize  ();    ///< Algorithm finalization

 

protected:

  /** @brief Collect hits in the x-layers. Obsolete, just kept for documentation
   *  @param part lower (1) or upper (0) half
   */
  void findXProjections( unsigned int part );

  /** @brief Collect hits in the stereo-layers. Obsolete, just kept for documentation
   *  @param part lower (1) or upper (0) half
   */
  void addStereo( unsigned int part );
  
  /** @brief Fit the track with a parabola
   *  @param track The track to fit
   *  @return bool Success of the fit
   */
  bool fitTrack( PrSeedTrack& track );

  /** @brief Remove the hit which gives the largest contribution to the chi2 and refit
   *  @param track The track to fit
   *  @return bool Success of the fit
   */
  bool removeWorstAndRefit( PrSeedTrack& track );
  
  /** @brief Set the chi2 of the track
   *  @param track The track to set the chi2 of 
   */
  void setChi2( PrSeedTrack& track );

  /** @brief Transform the tracks from the internal representation into LHCb::Tracks
   *  @param tracks The tracks to transform
   */
  void makeLHCbTracks( LHCb::Tracks* result );

  /** @brief Print some information of the hit in question
   *  @param hit The hit whose information should be printed
   *  @param title Some additional information to be printed
   */
  void printHit( const PrHit* hit, std::string title="" );

  /** @brief Print some information of the track in question
   *  @param hit The track whose information should be printed
   */
  void printTrack( PrSeedTrack& track );

  
  bool matchKey( const PrHit* hit ) {
    if ( m_debugTool ) return m_debugTool->matchKey( hit->id(), m_wantedKey );
    return false;
  };

  bool matchKey( const PrSeedTrack& track ) {
    if ( !m_debugTool ) return false;
    for ( PrHits::const_iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
      if ( m_debugTool->matchKey( (*itH)->id(), m_wantedKey ) ) return true;
    }
    return false;
  };


  /** @brief Collect hits in the x-layers using a parabolic search window.
   *  @param part lower (1) or upper (0) half
   */
  void findXProjections2( unsigned int part );
  
  /** @brief Collect hits in the stereo-layers.
   *  @param part lower (1) or upper (0) half
   */
  void addStereo2( unsigned int part );

  /** @brief Internal method to construct parabolic parametrisation out of three hits, using Cramer's rule.
   *  @param hit1 First hit
   *  @param hit2 Second hit
   *  @param hit3 Third hit
   *  @param a quadratic coefficient
   *  @param b linear coefficient
   *  @param c offset
   */
  void solveParabola(const PrHit* hit1, const PrHit* hit2, const PrHit* hit3, float& a, float& b, float& c);
  
  
  /// Class to find lower bound of x of PrHit
  class lowerBoundX {
  public:
    bool operator() (const PrHit* lhs, const double testval ) const { return lhs->x() < testval; }
  };

  /// Class to compare x positions of PrHits
  class compX {
  public:
    bool operator() (const PrHit* lhs, const PrHit* rhs ) const { return lhs->x() < rhs->x(); }
  };

  /// Class to find lower bound of LHCbIDs of PrHits
  class lowerBoundLHCbID {
  public:
    bool operator() (const PrHit* lhs, const LHCb::LHCbID id ) const { return lhs->id() < id; }
  };
  
  /// Class to compare LHCbIDs of PrHits
  class compLHCbID {
  public:
    bool operator() (const PrHit* lhs, const PrHit* rhs ) const { return lhs->id() < rhs->id(); }
  };

  
  
private:
  std::string     m_inputName;
  std::string     m_outputName;
  std::string     m_hitManagerName;
  float           m_maxChi2InTrack;
  float           m_maxIpAtZero;
  float           m_tolXInf;
  float           m_tolXSup;
  unsigned int    m_minXPlanes;
  float           m_maxChi2PerDoF;
  bool            m_xOnly;
  unsigned int    m_maxParabolaSeedHits;
  
  float           m_tolTyOffset;
  float           m_tolTySlope;
  
  bool            m_decodeData;
  bool            m_printSettings;
  

  PrHitManager*   m_hitManager;
  PrGeometryTool* m_geoTool;

  //== Debugging controls
  std::string     m_debugToolName;
  int             m_wantedKey;
  IPrDebugTool*   m_debugTool;

  std::vector<PrSeedTrack>       m_trackCandidates;
  std::vector<PrSeedTrack>       m_xCandidates;

  bool           m_doTiming;
  ISequencerTimerTool* m_timerTool;
  int            m_timeTotal;
  int            m_timeFromForward;
  int            m_timeXProjection;
  int            m_timeStereo;
  int            m_timeFinal;
};
#endif // PRSEEDINGALGORITHM_H
