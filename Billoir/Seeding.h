#ifndef PRSEEDINGXLAYERS_H 
    2 #define PRSEEDINGYLAYERS_H 1
    3 
    4 // Include files
    5 // from Gaudi
    6 #include "GaudiAlg/GaudiAlgorithm.h"
    7 #include "GaudiAlg/ISequencerTimerTool.h"
    8 
    9 #include "PrKernel/IPrDebugTool.h"
   10 #include "PrKernel/PrHitManager.h"
   11 #include "PrSeedTrack.h"
   12 #include "PrGeometryTool.h"
   13 #include "TfKernel/RecoFuncs.h"
   14 
   45 class PrSeedingXLayers : public GaudiAlgorithm {
   46 public:
   48   PrSeedingXLayers( const std::string& name, ISvcLocator* pSvcLocator );
   49 
   50   virtual ~PrSeedingXLayers( ); 
   51 
   52   virtual StatusCode initialize();    
   53   virtual StatusCode execute   ();    
   54   virtual StatusCode finalize  ();    
   55 
   56  
   57 
   58 protected:
   59 
   63   void findXProjections( unsigned int part );
   64 
   68   void addStereo( unsigned int part );
   69   
   74   bool fitTrack( PrSeedTrack& track );
   75 
   80   bool removeWorstAndRefit( PrSeedTrack& track );
   81   
   85   void setChi2( PrSeedTrack& track );
   86 
   90   void makeLHCbTracks( LHCb::Tracks* result );
   91 
   96   void printHit( const PrHit* hit, std::string title="" );
   97 
  101   void printTrack( PrSeedTrack& track );
  102 
  103   
  104   bool matchKey( const PrHit* hit ) {
  105     if ( m_debugTool ) return m_debugTool->matchKey( hit->id(), m_wantedKey );
  106     return false;
  107   };
  108 
  109   bool matchKey( const PrSeedTrack& track ) {
  110     if ( !m_debugTool ) return false;
  111     for ( PrHits::const_iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
  112       if ( m_debugTool->matchKey( (*itH)->id(), m_wantedKey ) ) return true;
  113     }
  114     return false;
  115   };
  116 
  117 
  121   void findXProjections2( unsigned int part );
  122   
  126   void addStereo2( unsigned int part );
  127 
  136   void solveParabola(const PrHit* hit1, const PrHit* hit2, const PrHit* hit3, float& a, float& b, float& c);
  137   
  138   
  140   class lowerBoundX {
  141   public:
  142     bool operator() (const PrHit* lhs, const double testval ) const { return lhs->x() < testval; }
  143   };
  144 
  146   class compX {
  147   public:
  148     bool operator() (const PrHit* lhs, const PrHit* rhs ) const { return lhs->x() < rhs->x(); }
  149   };
  150 
  152   class lowerBoundLHCbID {
  153   public:
  154     bool operator() (const PrHit* lhs, const LHCb::LHCbID id ) const { return lhs->id() < id; }
  155   };
  156   
  158   class compLHCbID {
  159   public:
  160     bool operator() (const PrHit* lhs, const PrHit* rhs ) const { return lhs->id() < rhs->id(); }
  161   };
  162 
  163   
  164   
  165 private:
  166   std::string     m_inputName;
  167   std::string     m_outputName;
  168   std::string     m_hitManagerName;
  169   float           m_maxChi2InTrack;
  170   float           m_maxIpAtZero;
  171   float           m_tolXInf;
  172   float           m_tolXSup;
  173   unsigned int    m_minXPlanes;
  174   float           m_maxChi2PerDoF;
  175   bool            m_xOnly;
  176   unsigned int    m_maxParabolaSeedHits;
  177   
  178   float           m_tolTyOffset;
  179   float           m_tolTySlope;
  180   
  181   bool            m_decodeData;
  182   bool            m_printSettings;
  183   
  184 
  185   PrHitManager*   m_hitManager;
  186   PrGeometryTool* m_geoTool;
  187 
  188   //== Debugging controls
  189   std::string     m_debugToolName;
  190   int             m_wantedKey;
  191   IPrDebugTool*   m_debugTool;
  192 
  193   std::vector<PrSeedTrack>       m_trackCandidates;
  194   std::vector<PrSeedTrack>       m_xCandidates;
  195 
  196   bool           m_doTiming;
  197   ISequencerTimerTool* m_timerTool;
  198   int            m_timeTotal;
  199   int            m_timeFromForward;
  200   int            m_timeXProjection;
  201   int            m_timeStereo;
  202   int            m_timeFinal;
  203 };
  204 #endif // PRSEEDINGALGORITHM_H
Generated on Mon Jun 23 2014 20:21:01 for LHCb Software by   doxygen 1.8.2
