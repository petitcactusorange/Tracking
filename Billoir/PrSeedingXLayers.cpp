// Include files 

// from Gaudi
#include "GaudiKernel/AlgFactory.h"
#include "Event/Track.h"
#include "Event/StateParameters.h"
// local
#include "PrSeedingXLayers.h"
#include "PrPlaneCounter.h"

//-----------------------------------------------------------------------------
// Implementation file for class : PrSeedingXLayers
//
// 2013-02-14 : Olivier Callot
// 2013-03-21 : Yasmine Amhis Modification 
//-----------------------------------------------------------------------------

// Declaration of the Algorithm Factory
DECLARE_ALGORITHM_FACTORY( PrSeedingXLayers )

//=============================================================================
// Standard constructor, initializes variables
//=============================================================================
PrSeedingXLayers::PrSeedingXLayers( const std::string& name,
                                        ISvcLocator* pSvcLocator)
: 
#ifdef DEBUG_HISTO
  GaudiTupleAlg(name, pSvcLocator),
#else
  GaudiAlgorithm ( name, pSvcLocator),
#endif
  m_hitManager(nullptr),
  m_geoTool(nullptr),
  m_debugTool(nullptr),
  m_timerTool(nullptr)
{
  declareProperty( "InputName",           m_inputName            = LHCb::TrackLocation::Forward );
  declareProperty( "OutputName",          m_outputName           = LHCb::TrackLocation::Seed    );
  declareProperty( "HitManagerName",      m_hitManagerName       = "PrFTHitManager"             );
  declareProperty( "DecodeData",          m_decodeData           = false                        );
  declareProperty( "XOnly",               m_xOnly                = false                        );
  
  declareProperty( "MaxChi2InTrack",      m_maxChi2InTrack       = 5.5                          );
  declareProperty( "TolXInf",             m_tolXInf              = 0.5 * Gaudi::Units::mm       );
  declareProperty( "TolXSup",             m_tolXSup              = 8.0 * Gaudi::Units::mm       );
  declareProperty( "MinXPlanes",          m_minXPlanes           = 5                            );
  declareProperty( "MaxChi2PerDoF",       m_maxChi2PerDoF        = 4.0                          );
  declareProperty( "MaxParabolaSeedHits", m_maxParabolaSeedHits  = 4                            );
  declareProperty( "TolTyOffset",         m_tolTyOffset          = 0.002                        );
  declareProperty( "TolTySlope",          m_tolTySlope           = 0.015                        );
  declareProperty( "MaxIpAtZero",         m_maxIpAtZero          = 5000.                        );
  
  // Parameters for debugging
  declareProperty( "DebugToolName",       m_debugToolName         = ""                          );
  declareProperty( "WantedKey",           m_wantedKey             = -100                        );
  declareProperty( "TimingMeasurement",   m_doTiming              = false                       );
  declareProperty( "PrintSettings",       m_printSettings         = false                       );
  
}
//=============================================================================
// Destructor
//=============================================================================
PrSeedingXLayers::~PrSeedingXLayers() {}

//=============================================================================
// Initialization
//=============================================================================
StatusCode PrSeedingXLayers::initialize() {
  StatusCode sc = GaudiAlgorithm::initialize(); // must be executed first
  if ( sc.isFailure() ) return sc;  // error printed already by GaudiAlgorithm

  if ( msgLevel(MSG::DEBUG) ) debug() << "==> Initialize" << endmsg;

  m_hitManager = tool<PrHitManager>( m_hitManagerName );
  m_hitManager->buildGeometry();
  m_geoTool = tool<PrGeometryTool>("PrGeometryTool");

  m_debugTool   = 0;
  if ( "" != m_debugToolName ) {
    m_debugTool = tool<IPrDebugTool>( m_debugToolName );
  } else {
    m_wantedKey = -100;  // no debug
  }

  if ( m_doTiming) {
    m_timerTool = tool<ISequencerTimerTool>( "SequencerTimerTool/Timer", this );
    m_timeTotal   = m_timerTool->addTimer( "PrSeeding total" );
    m_timerTool->increaseIndent();
    m_timeFromForward = m_timerTool->addTimer( "Convert Forward" );
    m_timeXProjection = m_timerTool->addTimer( "X Projection" );
    m_timeStereo      = m_timerTool->addTimer( "Add stereo" );
    m_timeFinal       = m_timerTool->addTimer( "Convert tracks" );
    m_timerTool->decreaseIndent();
  }

  if( m_decodeData ) info() << "Will decode the FT clusters!" << endmsg;

  // -- Print the settings of this algorithm in a readable way
  if( m_printSettings){
    
    info() << "========================================"             << endmsg
           << " InputName            = " <<  m_inputName             << endmsg
           << " OutputName           = " <<  m_outputName            << endmsg
           << " HitManagerName       = " <<  m_hitManagerName        << endmsg
           << " DecodeData           = " <<  m_decodeData            << endmsg
           << " XOnly                = " <<  m_xOnly                 << endmsg
           << " MaxChi2InTrack       = " <<  m_maxChi2InTrack        << endmsg
           << " TolXInf              = " <<  m_tolXInf               << endmsg
           << " TolXSup              = " <<  m_tolXSup               << endmsg
           << " MinXPlanes           = " <<  m_minXPlanes            << endmsg
           << " MaxChi2PerDoF        = " <<  m_maxChi2PerDoF         << endmsg
           << " MaxParabolaSeedHits  = " <<  m_maxParabolaSeedHits   << endmsg
           << " TolTyOffset          = " <<  m_tolTyOffset           << endmsg
           << " TolTySlope           = " <<  m_tolTySlope            << endmsg
           << " MaxIpAtZero          = " <<  m_maxIpAtZero           << endmsg
           << " DebugToolName        = " <<  m_debugToolName         << endmsg
           << " WantedKey            = " <<  m_wantedKey             << endmsg
           << " TimingMeasurement    = " <<  m_doTiming              << endmsg
           << "========================================"             << endmsg;
  }
  
#ifdef DEBUG_HISTO 
  setHistoTopDir("FT/");
#endif 


  return StatusCode::SUCCESS;
}

//=============================================================================
// Main execution
//=============================================================================
StatusCode PrSeedingXLayers::execute() {
  if ( msgLevel(MSG::DEBUG) ) debug() << "==> Execute" << endmsg;
  if ( m_doTiming ) {
    m_timerTool->start( m_timeTotal );
    m_timerTool->start( m_timeFromForward );
  }

  LHCb::Tracks* result = new LHCb::Tracks();
  put( result, m_outputName );

  // -- This is only needed if the seeding is the first algorithm using the FT
  // -- As the Forward normally runs first, it's off per default
  if( m_decodeData ) m_hitManager->decodeData();   
  int multiplicity = 0;
  for ( unsigned int zone = 0; m_hitManager->nbZones() > zone; ++zone ) {
    for ( PrHits::const_iterator itH = m_hitManager->hits( zone ).begin();
          m_hitManager->hits( zone ).end() != itH; ++itH ) {
      (*itH)->setUsed( false );
     multiplicity++;
    }
  }

  //== If needed, debug the cluster associated to the requested MC particle.
  if ( 0 <= m_wantedKey ) {
    info() << "--- Looking for MCParticle " << m_wantedKey << endmsg;
    for ( unsigned int zone = 0; m_hitManager->nbZones() > zone; ++zone ) {
      for ( PrHits::const_iterator itH = m_hitManager->hits( zone ).begin();
            m_hitManager->hits( zone ).end() != itH; ++itH ) {
        if ( matchKey( *itH ) ) printHit( *itH, " " );
      }
    }
  }
  //====================================================================
  // Extract the seed part from the forward tracks.
  //====================================================================
  if ( "" != m_inputName ) {
    
    // -- sort hits according to LHCbID
    for(int i = 0; i < 24; i++){
      PrHitZone* zone = m_hitManager->zone(i);
      std::stable_sort( zone->hits().begin(),  zone->hits().end(), compLHCbID());
    }
    // ------------------------------------------
    
    LHCb::Tracks* forward = get<LHCb::Tracks>( m_inputName );
    for ( LHCb::Tracks::iterator itT = forward->begin(); forward->end() != itT; ++itT ) {
      std::vector<LHCb::LHCbID> ids;
      ids.reserve(20);
      for ( std::vector<LHCb::LHCbID>::const_iterator itId = (*itT)->lhcbIDs().begin();
            (*itT)->lhcbIDs().end() != itId; ++itId ) {
        if ( (*itId).isFT() ) {
          LHCb::FTChannelID ftId =(*itId).ftID();
          int zoneNb = 2 * ftId.layer() + ftId.mat(); //zones top are even (0, 2, 4, ....,22)  and zones bottom are odd 
          PrHitZone* zone = m_hitManager->zone(zoneNb);
          // -- The hits are sorted according to LHCbID, we can therefore use a lower bound to speed up the search
          PrHits::iterator itH = std::lower_bound(  zone->hits().begin(),  zone->hits().begin(), *itId, lowerBoundLHCbID() );
	  
          for ( ; zone->hits().end() != itH; ++itH ) {
            if( *itId < (*itH)->id() ) break;
            if ( (*itH)->id() == *itId ) (*itH)->setUsed( true ); 
	  }
          ids.push_back( *itId );
        }
      }
      
      LHCb::Track* seed = new LHCb::Track;
      seed->setLhcbIDs( ids );
      seed->setType( LHCb::Track::Ttrack );
      seed->setHistory( LHCb::Track::PrSeeding );
      seed->setPatRecStatus( LHCb::Track::PatRecIDs );
      seed->addToStates( (*itT)->closestState( 9000. ) );
      result->insert( seed );
    }
    // -- sort hits according to x
   for(int i = 0; i < 24; i++){
    PrHitZone* zone = m_hitManager->zone(i); 
    std::stable_sort( zone->hits().begin(),  zone->hits().end(), compX());
    
   }



  }


  m_trackCandidates.clear();
  if ( m_doTiming ) {
    m_timerTool->stop( m_timeFromForward );
  }

  // -- Loop through lower and upper half
  for ( unsigned int part= 0; 2 > part; ++part ) {
    if ( m_doTiming ) {
      m_timerTool->start( m_timeXProjection);
    }
    findXProjections2( part );

    if ( m_doTiming ) {
      m_timerTool->stop( m_timeXProjection);
      m_timerTool->start( m_timeStereo);
    }

    if ( ! m_xOnly ) addStereo2( part );
    if ( m_doTiming ) {
      m_timerTool->stop( m_timeStereo);
    }
  }

  if ( m_doTiming ) {
    m_timerTool->start( m_timeFinal);
  }

  makeLHCbTracks( result );

  if ( m_doTiming ) {
    m_timerTool->stop( m_timeFinal);
    float tot = m_timerTool->stop( m_timeTotal );
    debug() << format( "                                            Time %8.3f ms", tot )<< endmsg;
    #ifdef DEBUG_HISTO
    plot2D(multiplicity, tot, "timing", "timing", 0, 10000, 0, 1000, 100, 100) ;
    #endif 

  }


  
  return StatusCode::SUCCESS;
}

//=============================================================================
//  Finalize
//=============================================================================
StatusCode PrSeedingXLayers::finalize() {

  if ( msgLevel(MSG::DEBUG) ) debug() << "==> Finalize" << endmsg;

  return GaudiAlgorithm::finalize();  // must be called after all other actions
}

//=========================================================================
//
//=========================================================================
void PrSeedingXLayers::findXProjections( unsigned int part ){
  m_xCandidates.clear();
  for ( unsigned int iCase = 0 ; 3 > iCase ; ++iCase ) {
    int firstZone = part;
    int lastZone  = 22 + part;
    if ( 1 == iCase ) firstZone = part + 6;
    if ( 2 == iCase ) lastZone  = 16 + part;
    
    PrHitZone* fZone = m_hitManager->zone( firstZone );
    PrHitZone* lZone = m_hitManager->zone( lastZone  );
    PrHits& fHits = fZone->hits();
    PrHits& lHits = lZone->hits();
    
   
    float zRatio =  lZone->z(0.) / fZone->z(0.);
    
  

    
    std::vector<PrHitZone*> xZones;
    xZones.reserve(12);
    for ( int kk = firstZone+2; lastZone > kk ; kk += 2 ) {
      if ( m_hitManager->zone( kk )->isX() ) xZones.push_back( m_hitManager->zone(kk) );
    }
    
    PrHits::iterator itLBeg = lHits.begin();

    // -- Define the iterators for the "in-between" layers
    std::vector< PrHits::iterator > iterators;
    iterators.reserve(24);
    
    for ( PrHits::iterator itF = fHits.begin(); fHits.end() != itF; ++itF ) {
      if ( 0 != iCase && (*itF)->isUsed() ) continue;
      float minXl = (*itF)->x() * zRatio - m_maxIpAtZero * ( zRatio - 1 );
      float maxXl = (*itF)->x() * zRatio + m_maxIpAtZero * ( zRatio - 1 );
    

      if ( matchKey( *itF ) ) info() << "Search from " << minXl << " to " << maxXl << endmsg;
      
      itLBeg = std::lower_bound( lHits.begin(), lHits.end(), minXl, lowerBoundX() );
      while ( itLBeg != lHits.end() && (*itLBeg)->x() < minXl ) {
        ++itLBeg;
        if ( lHits.end() == itLBeg ) break;
      }

      PrHits::iterator itL = itLBeg;
    
      // -- Initialize the iterators
      iterators.clear();
      for(std::vector<PrHitZone*>::iterator it = xZones.begin(); it != xZones.end(); it++){
        iterators.push_back( (*it)->hits().end() );
      }
      

      while ( itL != lHits.end() && (*itL)->x() < maxXl ) {
      
        
        if ( 0 != iCase && (*itL)->isUsed() ) {
          ++itL;
          continue;
        }
     
        
        float tx = ((*itL)->x() - (*itF)->x()) / (lZone->z() - fZone->z() );
        float x0 = (*itF)->x() - (*itF)->z() * tx;
        bool debug = matchKey( *itF ) || matchKey( *itL );
        PrHits xCandidate;
        xCandidate.reserve(30); // assume no more than 5 hits per layer
        xCandidate.push_back( *itF );
        int nMiss = 0;
        if ( 0 != iCase ) nMiss = 1;
        

        //loop over ALL x zones 
        unsigned int counter = 0; // counter to identify the iterators
        for ( std::vector<PrHitZone*>::iterator itZ = xZones.begin(); xZones.end() != itZ; ++itZ ) {
          
          float xP   = x0 + (*itZ)->z() * tx;
          float xMax = xP + m_tolXSup;
          float xMin = xP - m_tolXInf;
        
          if ( x0 < 0 ) {
            xMin = xP - m_tolXSup;
            xMax = xP + m_tolXInf;
          }
  
          // -- If iterator is not set to any position, search lower bound for it
          if(  iterators.at( counter ) == (*itZ)->hits().end() ){
            iterators.at( counter ) =  std::lower_bound( (*itZ)->hits().begin(), (*itZ)->hits().end(), xMin, lowerBoundX() );
          }
          PrHits::iterator itH = iterators.at( counter );
          
          float xMinStrict = xP - m_tolXSup;
          PrHit* best = nullptr;

          for ( ; (*itZ)->hits().end() != itH; ++itH ) {
            
            // -- Advance the iterator, if it is lower than any possible bound 
            //-- (as lower bound could change, need to use strict criterion here)
            if ( (*itH)->x() < xMinStrict ){
              iterators.at( counter )++;
              continue;
            }
            
            if ( (*itH)->x() < xMin ) continue;
            if ( (*itH)->x() > xMax ) break;
            
            best = *itH;
            xCandidate.push_back( best );
          }
          if ( nullptr == best ) {
            nMiss++;
            if ( 1 < nMiss ) break;
          }
          counter++;
 
        }
        

        if ( nMiss < 2 ) {
          xCandidate.push_back( *itL );
          PrSeedTrack temp( part, m_geoTool->zReference(), xCandidate );
          
          bool OK = fitTrack( temp );
          if ( debug ) {
            info() << "=== before fit === OK = " << OK << endmsg;
            printTrack( temp );
          }
          
          while ( !OK ) {
            OK = removeWorstAndRefit( temp );
          }
          setChi2( temp );
          if ( debug ) {
            info() << "=== after fit === chi2/dof = " << temp.chi2PerDoF() << endmsg;
            printTrack( temp );
          }
          if ( OK && 
               temp.hits().size() >= m_minXPlanes &&
               temp.chi2PerDoF()  < m_maxChi2PerDoF   ) {
            if ( temp.hits().size() == 6 ) {
              for ( PrHits::iterator itH = temp.hits().begin(); temp.hits().end() != itH; ++ itH) {
                (*itH)->setUsed( true );
              }
            }
            
            m_xCandidates.push_back( temp );
            if ( debug ) {
              info() << "Candidate chi2PerDoF " << temp.chi2PerDoF() << endmsg;
              printTrack( temp );
            }
          } else {
            if ( debug ) info() << "OK " << OK << " size " << temp.hits().size() << " chi2 " << temp.chi2() << endmsg;
          }
        }
        ++itL;
      }
    }
  }

  std::stable_sort( m_xCandidates.begin(), m_xCandidates.end(), PrSeedTrack::GreaterBySize() );

  //====================================================================
  // Remove clones, i.e. share more than 2 hits
  //====================================================================
  for ( PrSeedTracks::iterator itT1 = m_xCandidates.begin(); m_xCandidates.end() !=itT1; ++itT1 ) {
    if ( !(*itT1).valid() ) continue;
    if ( (*itT1).hits().size() != 6 ) {
      int nUsed = 0;
      for ( PrHits::iterator itH = (*itT1).hits().begin(); (*itT1).hits().end() != itH; ++ itH) {
        if ( (*itH)->isUsed() ) ++nUsed;
      }
      if ( 2 < nUsed ) {
        (*itT1).setValid( false );
        continue;
      }
    }    

    for ( PrSeedTracks::iterator itT2 = itT1 + 1; m_xCandidates.end() !=itT2; ++itT2 ) {
      if ( !(*itT2).valid() ) continue;
      int nCommon = 0;
      PrHits::iterator itH1 = (*itT1).hits().begin();
      PrHits::iterator itH2 = (*itT2).hits().begin();
      while ( itH1 != (*itT1).hits().end() && itH2 != (*itT2).hits().end() ) {
        if ( (*itH1)->id() == (*itH2)->id() ) {
          nCommon++;
          ++itH1;
          ++itH2;
        } else if ( (*itH1)->id() < (*itH2)->id() ) {
          ++itH1;
        } else {
          ++itH2;
        }
      }
      if ( nCommon > 2 ) {
        if ( (*itT1).hits().size() > (*itT2).hits().size() ) {
          (*itT2).setValid( false );
        } else if ( (*itT1).hits().size() < (*itT2).hits().size() ) {
          (*itT1).setValid( false );
        } else if ( (*itT1).chi2PerDoF() < (*itT2).chi2PerDoF() ) {
          (*itT2).setValid( false );
        } else {
          (*itT1).setValid( false );
        }
      }
    }
    if ( m_xOnly ) m_trackCandidates.push_back( *itT1 );
  }
}

//=========================================================================
// Add Stereo hits
//=========================================================================
void PrSeedingXLayers::addStereo( unsigned int part ) {
  PrSeedTracks xProjections;
  for ( PrSeedTracks::iterator itT1 = m_xCandidates.begin(); m_xCandidates.end() !=itT1; ++itT1 ) {
    if ( !(*itT1).valid() ) continue;
    xProjections.push_back( *itT1 );
  }

  unsigned int firstZone = part + 2;
  unsigned int lastZone  = part + 22;
  for ( PrSeedTracks::iterator itT = xProjections.begin(); xProjections.end() !=itT; ++itT ) {
    bool debug = false;
    int nMatch = 0;
    for ( PrHits::iterator itH = (*itT).hits().begin(); (*itT).hits().end() != itH; ++itH ) {
      if (  matchKey( *itH ) ) ++nMatch;
    }
    debug = nMatch > 3;
    if ( debug ) {
      info() << "Processing track " << endmsg;
      printTrack( *itT );
    }
    
    PrHits myStereo;
    myStereo.reserve(30);
    for ( unsigned int kk = firstZone; lastZone > kk ; kk+= 2 ) {
      if ( m_hitManager->zone(kk)->isX() ) continue;
      float dxDy = m_hitManager->zone(kk)->dxDy();
      float zPlane = m_hitManager->zone(kk)->z();
      
      float xPred = (*itT).x( m_hitManager->zone(kk)->z() );
      float xMin = xPred + 2700. * dxDy;
      float xMax = xPred - 2700. * dxDy;
      if ( xMin > xMax ) {
        float tmp = xMax;
        xMax = xMin;
        xMin = tmp;
      }

      PrHits::iterator itH = std::lower_bound( m_hitManager->zone(kk)->hits().begin(),  
                                               m_hitManager->zone(kk)->hits().end(), xMin, lowerBoundX() );
      //      for ( PrHits::iterator itH = m_hitManager->zone(kk)->hits().begin();
      for ( ;
            m_hitManager->zone(kk)->hits().end() != itH; ++itH ) {
        if ( (*itH)->x() < xMin ) continue;
        if ( (*itH)->x() > xMax ) break;
        (*itH)->setCoord( ((*itH)->x() - xPred) / dxDy / zPlane );
        if ( debug ) {
          if ( matchKey( *itH ) ) printHit( *itH, "" );
        }
        if ( 0 == part && (*itH)->coord() < -0.005 ) continue;
        if ( 1 == part && (*itH)->coord() >  0.005 ) continue;
        myStereo.push_back( *itH );
      }
    }
    std::stable_sort( myStereo.begin(), myStereo.end(), PrHit::LowerByCoord() );

    PrPlaneCounter plCount;
    unsigned int firstSpace = m_trackCandidates.size();

    PrHits::iterator itBeg = myStereo.begin();
    PrHits::iterator itEnd = itBeg + 5;
    
    while ( itEnd < myStereo.end() ) {
      float tolTy = 0.002 + .02 * fabs( (*itBeg)->coord() );
      if ( (*(itEnd-1))->coord() - (*itBeg)->coord() < tolTy ) {
        while( itEnd+1 < myStereo.end() &&
               (*itEnd)->coord() - (*itBeg)->coord() < tolTy ) {
          ++itEnd;
        }
        plCount.set( itBeg, itEnd );
        if ( 4 < plCount.nbDifferent() ) {
          PrSeedTrack temp( *itT );
          for ( PrHits::iterator itH = itBeg; itEnd != itH; ++itH ) temp.addHit( *itH );
          bool ok = fitTrack( temp );
          ok = fitTrack( temp );
          ok = fitTrack( temp );
          if ( debug ) {
            info() << "Before fit-and-remove" << endmsg;
            printTrack( temp );
          }
          while ( !ok && temp.hits().size() > 10 ) {
            ok = removeWorstAndRefit( temp );
            if ( debug ) {
              info() << "    after removing worse" << endmsg;
              printTrack( temp );
            }
          }
          if ( ok ) {
            setChi2( temp );
            if ( debug ) {
              info() << "=== Candidate chi2 " << temp.chi2PerDoF() << endmsg;
              printTrack( temp );
            }
            if ( temp.hits().size() > 9 || 
                 temp.chi2PerDoF() < m_maxChi2PerDoF ) {
              m_trackCandidates.push_back( temp );
            }
            itBeg += 4;
          }
        }
      }
      ++itBeg;
      itEnd = itBeg + 5;
    }
    //=== Remove bad candidates: Keep the best for this input track
    if ( m_trackCandidates.size() > firstSpace+1 ) {
      for ( unsigned int kk = firstSpace; m_trackCandidates.size()-1 > kk ; ++kk ) {
        if ( !m_trackCandidates[kk].valid() ) continue;
        for ( unsigned int ll = kk + 1; m_trackCandidates.size() > ll; ++ll ) {
          if ( !m_trackCandidates[ll].valid() ) continue;
          if ( m_trackCandidates[ll].hits().size() < m_trackCandidates[kk].hits().size() ) {
            m_trackCandidates[ll].setValid( false );
          } else if ( m_trackCandidates[ll].hits().size() > m_trackCandidates[kk].hits().size() ) {
            m_trackCandidates[kk].setValid( false );
          } else if ( m_trackCandidates[kk].chi2() < m_trackCandidates[ll].chi2() ) {
            m_trackCandidates[ll].setValid( false );
          } else {
            m_trackCandidates[kk].setValid( false );
          }  
        }   
      }
    }
  }
}

//=========================================================================
//  Fit the track, return OK if fit sucecssfull
//=========================================================================
bool PrSeedingXLayers::fitTrack( PrSeedTrack& track ) {

  for ( int loop = 0; 3 > loop ; ++loop ) {
    //== Fit a parabola
    float s0   = 0.;
    float sz   = 0.;
    float sz2  = 0.;
    float sz3  = 0.;
    float sz4  = 0.;
    float sd   = 0.;
    float sdz  = 0.;
    float sdz2 = 0.;
    float sdz3 = 0.;

    float t0  = 0.;
    float tz  = 0.;
    float tz2 = 0.;
    float td  = 0.;
    float tdz = 0.;

    for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
      float w = (*itH)->w();
      float z = (*itH)->z() - m_geoTool->zReference();
      if ( (*itH)->dxDy() != 0 ) {
        if ( 0 == loop ) continue;
        float dy = track.deltaY( *itH );
        t0   += w;
        tz   += w * z;
        tz2  += w * z * z;
        td   += w * dy;
        tdz  += w * dy * z;
      }
      float d = track.distance( *itH );
      s0   += w;
      sz   += w * z;
      sz2  += w * z * z;
      sz3  += w * z * z * z;
      sz4  += w * z * z * z * z;
      sd   += w * d;
      sdz  += w * d * z;
      sdz2 += w * d * z * z;
      sdz3 += w * d * z * z;
    }
    float b1 = sz  * sz  - s0  * sz2;
    float c1 = sz2 * sz  - s0  * sz3;
    float d1 = sd  * sz  - s0  * sdz;
    float b2 = sz2 * sz2 - sz * sz3;
    float c2 = sz3 * sz2 - sz * sz4;
    float d2 = sdz * sz2 - sz * sdz2;

    float den = (b1 * c2 - b2 * c1 );
    if( fabs(den) < 1e-9 ) return false;
    float db  = (d1 * c2 - d2 * c1 ) / den;
    float dc  = (d2 * b1 - d1 * b2 ) / den;
    float da  = ( sd - db * sz - dc * sz2 ) / s0;

    float day = 0.;
    float dby = 0.;
    if ( t0 > 0. ) {
      float deny = (tz  * tz - t0 * tz2);
      day = -(tdz * tz - td * tz2) / deny;
      dby = -(td  * tz - t0 * tdz) / deny;
    }

    track.updateParameters( da, db, dc, day, dby );
    float maxChi2 = 0.;
    for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
      float chi2 = track.chi2( *itH );
      if ( chi2 > maxChi2 ) {
        maxChi2 = chi2;
      }
    }
    if ( m_maxChi2InTrack > maxChi2 ) return true;
  }
  return false;
}

//=========================================================================
//  Remove the worst hit and refit.
//=========================================================================
bool PrSeedingXLayers::removeWorstAndRefit ( PrSeedTrack& track ) {
  float maxChi2 = 0.;
  PrHits::iterator worst = track.hits().begin();
  for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
    float chi2 = track.chi2( *itH );
    if ( chi2 > maxChi2 ) {
      maxChi2 = chi2;
      worst = itH;
    }
  }
  track.hits().erase( worst );
  return fitTrack( track );
}
//=========================================================================
//  Set the chi2 of the track
//=========================================================================
void PrSeedingXLayers::setChi2 ( PrSeedTrack& track ) {
  float chi2 = 0.;
  int   nDoF = -3;  // Fitted a parabola
  bool hasStereo = false;
  for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
    float d = track.distance( *itH );
    if ( (*itH)->dxDy() != 0 ) hasStereo = true;
    float w = (*itH)->w();
    chi2 += w * d * d;
    nDoF += 1;
  }
  if ( hasStereo ) nDoF -= 2;
  track.setChi2( chi2, nDoF );
}

//=========================================================================
//  Convert to LHCb tracks
//=========================================================================
void PrSeedingXLayers::makeLHCbTracks ( LHCb::Tracks* result ) {
  for ( PrSeedTracks::iterator itT = m_trackCandidates.begin();
        m_trackCandidates.end() != itT; ++itT ) {
    if ( !(*itT).valid() ) continue;

    //info() << "==== Store track ==== chi2/dof " << (*itT).chi2PerDoF() << endmsg;
    //printTrack( *itT );
    
    LHCb::Track* tmp = new LHCb::Track;
    //tmp->setType( LHCb::Track::Long );
    //tmp->setHistory( LHCb::Track::PatForward );
    tmp->setType( LHCb::Track::Ttrack );
    tmp->setHistory( LHCb::Track::PrSeeding );
    double qOverP = m_geoTool->qOverP( *itT );

    LHCb::State tState;
    double z = StateParameters::ZEndT;
    tState.setLocation( LHCb::State::AtT );
    tState.setState( (*itT).x( z ), (*itT).y( z ), z, (*itT).xSlope( z ), (*itT).ySlope( ), qOverP );

    //== overestimated covariance matrix, as input to the Kalman fit

    tState.setCovariance( m_geoTool->covariance( qOverP ) );
    tmp->addToStates( tState );

    //== LHCb ids.

    tmp->setPatRecStatus( LHCb::Track::PatRecIDs );
    for ( PrHits::iterator itH = (*itT).hits().begin(); (*itT).hits().end() != itH; ++itH ) {
      tmp->addToLhcbIDs( (*itH)->id() );
    }
    tmp->setChi2PerDoF( (*itT).chi2PerDoF() );
    tmp->setNDoF(       (*itT).nDoF() );
    result->insert( tmp );
  }
}

//=========================================================================
//  Print the information of the selected hit
//=========================================================================
void PrSeedingXLayers::printHit ( const PrHit* hit, std::string title ) {
  info() << "  " << title
         << format( "Plane%3d zone%2d z0 %8.2f x0 %8.2f  size%2d charge%3d coord %8.3f used%2d ",
                    hit->planeCode(), hit->zone(), hit->z(), hit->x(),
                    hit->size(), hit->charge(), hit->coord(), hit->isUsed() );
  if ( m_debugTool ) m_debugTool->printKey( info(), hit->id() );
  if ( matchKey( hit ) ) info() << " ***";
  info() << endmsg;
}

//=========================================================================
//  Print the whole track
//=========================================================================
void PrSeedingXLayers::printTrack ( PrSeedTrack& track ) {
  for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
    info() << format( "dist %7.3f dy %7.2f chi2 %7.2f ", track.distance( *itH ), track.deltaY( *itH ), track.chi2( *itH ) );
    printHit( *itH );
  }
}
//=========================================================================
// modified method to find the x projections
//=========================================================================
void PrSeedingXLayers::findXProjections2( unsigned int part ){
  m_xCandidates.clear();
  for ( unsigned int iCase = 0 ; 3 > iCase ; ++iCase ) {
    int firstZone = part;
    int lastZone  = 22 + part;
    if ( 1 == iCase ) firstZone = part + 6;
    if ( 2 == iCase ) lastZone  = 16 + part;
    
    PrHitZone* fZone = m_hitManager->zone( firstZone );
    PrHitZone* lZone = m_hitManager->zone( lastZone  );
    PrHits& fHits = fZone->hits();
    PrHits& lHits = lZone->hits();
    
    
    float zRatio =  lZone->z(0.) / fZone->z(0.);
    
    #ifdef DEBUG_HISTO
    plot(zRatio, "zRatio", "zRatio", 0., 2, 100);
    plot(fZone->hits().size(), "NumberOfHitsInFirstZone","NumberOfHitsInFirstZone", 0., 600., 100);
    plot(lZone->hits().size(), "NumberOfHitsInLastZone","NumberOfHitsInLastZone", 0., 600., 100);
    #endif
    
    
    
    std::vector<PrHitZone*> xZones;
    xZones.reserve(12);
    for ( int kk = firstZone+2; lastZone > kk ; kk += 2 ) {
      if ( m_hitManager->zone( kk )->isX() ) xZones.push_back( m_hitManager->zone(kk) );
    }
    
    PrHits::iterator itLBeg = lHits.begin();

    // -- Define the iterators for the "in-between" layers
    std::vector< PrHits::iterator > iterators;
    iterators.reserve(24);
    
    for ( PrHits::iterator itF = fHits.begin(); fHits.end() != itF; ++itF ) {
      
      if ( 0 != iCase && (*itF)->isUsed() ) continue;
      
      
      float minXl = (*itF)->x() * zRatio - m_maxIpAtZero * ( zRatio - 1 );
      float maxXl = (*itF)->x() * zRatio + m_maxIpAtZero * ( zRatio - 1 );
      #ifdef DEBUG_HISTO
      plot(minXl, "minXl", "minXl", -6000, 6000, 100); 
      plot(maxXl, "maxXl", "maxXl", -6000, 6000, 100);
      #endif
      if ( matchKey( *itF ) ) info() << "Search from " << minXl << " to " << maxXl << endmsg;
      
      itLBeg = std::lower_bound( lHits.begin(), lHits.end(), minXl, lowerBoundX() );
      while ( itLBeg != lHits.end() && (*itLBeg)->x() < minXl ) {
	
	++itLBeg;
        if ( lHits.end() == itLBeg ) break;
      }

      PrHits::iterator itL = itLBeg;
    
      while ( itL != lHits.end() && (*itL)->x() < maxXl ) {
      
        
        if ( 0 != iCase && (*itL)->isUsed()) {
          ++itL;
          continue;
        }
     
        
        float tx = ((*itL)->x() - (*itF)->x()) / (lZone->z() - fZone->z() );
        float x0 = (*itF)->x() - (*itF)->z() * tx;
	
	#ifdef DEBUG_HISTO
	plot(tx, "tx", "tx", -1.,1., 100 );
	plot(x0, "x0", "x0", 0., 6000., 100.);
	#endif
        PrHits parabolaSeedHits;
        parabolaSeedHits.clear();
        parabolaSeedHits.reserve(5);
        
        // -- loop over first two x zones
        // --------------------------------------------------------------------------------
        unsigned int counter = 0; 
        bool skip = true;
        if( iCase != 0 ) skip = false;
	for ( std::vector<PrHitZone*>::iterator itZ = xZones.begin(); xZones.end() != itZ; ++itZ ) {
	  ++counter;
          // -- to make sure, in case = 0, only x layers of the 2nd T station are used
          if(skip){
            skip = false;
            continue;
          }
	  if( iCase == 0){
            if(counter > 3) break;
          }else{
            if(counter > 2) break;
          }
          
	  float xP   = x0 + (*itZ)->z() * tx;
          float xMax = xP + 2*fabs(tx)*m_tolXSup + 1.5;
          float xMin = xP - m_tolXInf;

          #ifdef DEBUG_HISTO
	  plot(xP, "xP_x0pos", "xP_x0pos", -10000., 10000., 100); 
	  plot(xMax, "xMax_x0pos", "xMax_x0pos", -10000., 10000., 100);
	  plot(xMin, "xMin_x0pos", "xMax_x0pos", -10000., 10000., 100);
	  #endif

	  
          if ( x0 < 0 ) {
            xMin = xP - 2*fabs(tx)*m_tolXSup - 1.5;
            xMax = xP + m_tolXInf;
            #ifdef DEBUG_HISTO
	    plot(xP, "xP_x0neg", "xP_x0neg", -10000., 10000., 100); 
	    plot(xMax, "xMax_x0neg", "xMax_x0neg", -10000., 10000., 100);
	    plot(xMin, "xMin_x0neg", "xMax_x0neg", -10000., 10000., 100);
            #endif
	    
          }
         
          PrHits::iterator itH = std::lower_bound( (*itZ)->hits().begin(), (*itZ)->hits().end(), xMin, lowerBoundX() );
          for ( ; (*itZ)->hits().end() != itH; ++itH ) {
            
            if ( (*itH)->x() < xMin ) continue;
            if ( (*itH)->x() > xMax ) break;
            
            parabolaSeedHits.push_back( *itH );
          }
        }
        // --------------------------------------------------------------------------------

        debug() << "We have " << parabolaSeedHits.size() << " hits to seed the parabolas" << endmsg;
	#ifdef DEBUG_HISTO
	plot(parabolaSeedHits.size() , "HitsToSeedParabolas", "HitsToSeedParabolas", 0., 20., 20 );
         #endif 


        std::vector<PrHits> xHitsLists;
        xHitsLists.clear();


        // -- float xP   = x0 + (*itZ)->z() * tx;
        // -- Alles klar, Herr Kommissar?
        // -- The power of Lambda functions!
        // -- Idea is to reduce ghosts in very busy events and prefer the high momentum tracks
        // -- For this, the seedHits are storted according to their distance to the linear extrapolation
        // -- so that the ones with the least distance can be chosen in the end
        std::stable_sort( parabolaSeedHits.begin(), parabolaSeedHits.end(), 
                   [x0,tx](const PrHit* lhs, const PrHit* rhs)
                   ->bool{return fabs(lhs->x() - (x0+lhs->z()*tx)) <  fabs(rhs->x() - (x0+rhs->z()*tx)); });
        
        
        unsigned int maxParabolaSeedHits = m_maxParabolaSeedHits;
        if( parabolaSeedHits.size() < m_maxParabolaSeedHits){
          maxParabolaSeedHits = parabolaSeedHits.size();
        }
        

        for(unsigned int i = 0; i < maxParabolaSeedHits; ++i){
          
          float a = 0;
          float b = 0;
          float c = 0;
          
          PrHits xHits;
          xHits.clear();
          
          
          // -- formula is: x = a*dz*dz + b*dz + c = x, with dz = z - zRef
          solveParabola( *itF, parabolaSeedHits[i], *itL, a, b, c);
          
          debug() << "parabola equation: x = " << a << "*z^2 + " << b << "*z + " << c << endmsg;
          

          for ( std::vector<PrHitZone*>::iterator itZ = xZones.begin(); xZones.end() != itZ; ++itZ ) {
          
            float dz = (*itZ)->z() - m_geoTool->zReference();
            float xAtZ = a*dz*dz + b*dz + c;
            
            float xP   = x0 + (*itZ)->z() * tx;
            float xMax = xAtZ + fabs(tx)*2.0 + 0.5;
            float xMin = xAtZ - fabs(tx)*2.0 - 0.5;
                        
            
            debug() << "x prediction (linear): " << xP <<  "x prediction (parabola): " << xAtZ << endmsg;
            
            
            // -- Only use one hit per layer, which is closest to the parabola!
            PrHit* best = nullptr;
            float bestDist = 10.0;
            
            
            PrHits::iterator itH = std::lower_bound( (*itZ)->hits().begin(), (*itZ)->hits().end(), xMin, lowerBoundX() );
            for (; (*itZ)->hits().end() != itH; ++itH ) {
              
              if ( (*itH)->x() < xMin ) continue;
              if ( (*itH)->x() > xMax ) break;
              
              
              if( fabs((*itH)->x() - xAtZ ) < bestDist){
                bestDist = fabs((*itH)->x() - xAtZ );
                best = *itH;
              }
              
            }
            if( best != nullptr) xHits.push_back( best );
          }
          
          xHits.push_back( *itF );
          xHits.push_back( *itL );
          

          if( xHits.size() < 5) continue;
          std::stable_sort(xHits.begin(), xHits.end(), compX());
       
          
          bool isEqual = false;
          
          for( PrHits hits : xHitsLists){
            if( hits == xHits ){
              isEqual = true;
              break;
            }
          }
          

          if( !isEqual ) xHitsLists.push_back( xHits );
        }
        
        
        debug() << "xHitsLists size before removing duplicates: " << xHitsLists.size() << endmsg;
        
        // -- remove duplicates
        
        if( xHitsLists.size() > 2){
          std::stable_sort( xHitsLists.begin(), xHitsLists.end() );
          xHitsLists.erase( std::unique(xHitsLists.begin(), xHitsLists.end()), xHitsLists.end());
        }
        
        debug() << "xHitsLists size after removing duplicates: " << xHitsLists.size() << endmsg;
        

        
        for( PrHits xHits : xHitsLists ){
          
          PrSeedTrack temp( part, m_geoTool->zReference(), xHits );
          
          bool OK = fitTrack( temp );
          
          while ( !OK ) {
            OK = removeWorstAndRefit( temp );
          }
          setChi2( temp );
          // ---------------------------------------
  
          float maxChi2 = m_maxChi2PerDoF + 6*tx*tx;
          

          if ( OK && 
               temp.hits().size() >= m_minXPlanes &&
               temp.chi2PerDoF()  < maxChi2   ) {
            if ( temp.hits().size() == 6 ) {
              for ( PrHits::iterator itH = temp.hits().begin(); temp.hits().end() != itH; ++ itH) {
                (*itH)->setUsed( true );
              }
              
              
            }
            
            m_xCandidates.push_back( temp );
          }
          // -------------------------------------
        }
        ++itL;
      }
    }
  }
  
  
  std::stable_sort( m_xCandidates.begin(), m_xCandidates.end(), PrSeedTrack::GreaterBySize() );

  //====================================================================
  // Remove clones, i.e. share more than 2 hits
  //====================================================================
  for ( PrSeedTracks::iterator itT1 = m_xCandidates.begin(); m_xCandidates.end() !=itT1; ++itT1 ) {
    if ( !(*itT1).valid() ) continue;
    if ( (*itT1).hits().size() != 6 ) {
      int nUsed = 0;
      for ( PrHits::iterator itH = (*itT1).hits().begin(); (*itT1).hits().end() != itH; ++ itH) {
        if ( (*itH)->isUsed()) ++nUsed;
      }
      if ( 1 < nUsed ) {
        (*itT1).setValid( false );
        continue;
      }
    }    
    
    for ( PrSeedTracks::iterator itT2 = itT1 + 1; m_xCandidates.end() !=itT2; ++itT2 ) {
      if ( !(*itT2).valid() ) continue;
      int nCommon = 0;
      PrHits::iterator itH1 = (*itT1).hits().begin();
      PrHits::iterator itH2 = (*itT2).hits().begin();
      
      PrHits::iterator itEnd1 = (*itT1).hits().end();
      PrHits::iterator itEnd2 = (*itT2).hits().end();

      while ( itH1 != itEnd1 && itH2 != itEnd2 ) {
        if ( (*itH1)->id() == (*itH2)->id() ) {
          ++nCommon;
          ++itH1;
          ++itH2;
        } else if ( (*itH1)->id() < (*itH2)->id() ) {
          ++itH1;
        } else {
          ++itH2;
        }
      }
      if ( nCommon > 2 ) {
        if ( (*itT1).hits().size() > (*itT2).hits().size() ) {
          (*itT2).setValid( false );
        } else if ( (*itT1).hits().size() < (*itT2).hits().size() ) {
          (*itT1).setValid( false );
        } else if ( (*itT1).chi2PerDoF() < (*itT2).chi2PerDoF() ) {
          (*itT2).setValid( false );
        } else {
          (*itT1).setValid( false );
        }
      }
    }
    if ( m_xOnly ) m_trackCandidates.push_back( *itT1 );
  }
}
//=========================================================================
// Modified version of adding the stereo layers
//=========================================================================
void PrSeedingXLayers::addStereo2( unsigned int part ) {
  PrSeedTracks xProjections;
  for ( PrSeedTracks::iterator itT1 = m_xCandidates.begin(); m_xCandidates.end() !=itT1; ++itT1 ) {
    if ( !(*itT1).valid() ) continue;
    xProjections.push_back( *itT1 );
  }

  unsigned int firstZone = part + 2;
  unsigned int lastZone  = part + 22;
  for ( PrSeedTracks::iterator itT = xProjections.begin(); xProjections.end() !=itT; ++itT ) {
    
    PrHits myStereo;
    myStereo.reserve(30);
    for ( unsigned int kk = firstZone; lastZone > kk ; kk+= 2 ) {
      if ( m_hitManager->zone(kk)->isX() ) continue;
      float dxDy = m_hitManager->zone(kk)->dxDy();
      float zPlane = m_hitManager->zone(kk)->z();

      float xPred = (*itT).x( m_hitManager->zone(kk)->z() );

      float xMin = xPred + 2500. * dxDy;
      float xMax = xPred - 2500. * dxDy;
      
      if ( xMin > xMax ) {
        float tmp = xMax;
        xMax = xMin;
        xMin = tmp;
      }
      

      PrHits::iterator itH = std::lower_bound( m_hitManager->zone(kk)->hits().begin(),  
                                               m_hitManager->zone(kk)->hits().end(), xMin, lowerBoundX() );
      
      for ( ;
            m_hitManager->zone(kk)->hits().end() != itH; ++itH ) {
        
        if ( (*itH)->x() < xMin ) continue;
        if ( (*itH)->x() > xMax ) break;
      
        (*itH)->setCoord( ((*itH)->x() - xPred) / dxDy  / zPlane );
        
        if ( 1 == part && (*itH)->coord() < -0.005 ) continue;
        if ( 0 == part && (*itH)->coord() >  0.005 ) continue;

        myStereo.push_back( *itH );
      }
    }
    std::stable_sort( myStereo.begin(), myStereo.end(), PrHit::LowerByCoord() );

    PrPlaneCounter plCount;
    unsigned int firstSpace = m_trackCandidates.size();

    PrHits::iterator itBeg = myStereo.begin();
    PrHits::iterator itEnd = itBeg + 5;
    
    while ( itEnd < myStereo.end() ) {
    
      float tolTy = m_tolTyOffset + m_tolTySlope * fabs( (*itBeg)->coord() );

        if ( (*(itEnd-1))->coord() - (*itBeg)->coord() < tolTy ) {
          while( itEnd+1 < myStereo.end() &&
                 (*itEnd)->coord() - (*itBeg)->coord() < tolTy ) {
            ++itEnd;
          }
          
        
          plCount.set( itBeg, itEnd );
          if ( 4 < plCount.nbDifferent() ) {
            PrSeedTrack temp( *itT );
            for ( PrHits::iterator itH = itBeg; itEnd != itH; ++itH ) temp.addHit( *itH );
            bool ok = fitTrack( temp );
            ok = fitTrack( temp );
            ok = fitTrack( temp );
            
        
            while ( !ok && temp.hits().size() > 10 ) {
              ok = removeWorstAndRefit( temp );
            }
            if ( ok ) {
              setChi2( temp );
              
              float maxChi2 = m_maxChi2PerDoF + 6*temp.xSlope(9000)*temp.xSlope(9000);

              if ( temp.hits().size() > 9 || 
                   temp.chi2PerDoF() < maxChi2 ) {
                m_trackCandidates.push_back( temp );
                
              }
              itBeg += 4;
            }
          }
        }
        ++itBeg;
        itEnd = itBeg + 5;
    }
    

    
    //=== Remove bad candidates: Keep the best for this input track
    if ( m_trackCandidates.size() > firstSpace+1 ) {
      for ( unsigned int kk = firstSpace; m_trackCandidates.size()-1 > kk ; ++kk ) {
        if ( !m_trackCandidates[kk].valid() ) continue;
        for ( unsigned int ll = kk + 1; m_trackCandidates.size() > ll; ++ll ) {
          if ( !m_trackCandidates[ll].valid() ) continue;
          if ( m_trackCandidates[ll].hits().size() < m_trackCandidates[kk].hits().size() ) {
            m_trackCandidates[ll].setValid( false );
          } else if ( m_trackCandidates[ll].hits().size() > m_trackCandidates[kk].hits().size() ) {
            m_trackCandidates[kk].setValid( false );
          } else if ( m_trackCandidates[kk].chi2() < m_trackCandidates[ll].chi2() ) {
            m_trackCandidates[ll].setValid( false );
          } else {
            m_trackCandidates[kk].setValid( false );
          }  
        }   
      }
    }
    
  }
}


//=========================================================================
// Solve parabola using Cramer's rule 
//========================================================================
void PrSeedingXLayers::solveParabola(const PrHit* hit1, const PrHit* hit2, const PrHit* hit3, float& a, float& b, float& c){
  
  const float z1 = hit1->z() - m_geoTool->zReference();
  const float z2 = hit2->z() - m_geoTool->zReference();
  const float z3 = hit3->z() - m_geoTool->zReference();
  
  const float x1 = hit1->x();
  const float x2 = hit2->x();
  const float x3 = hit3->x();
  
  
  const float det = (z1*z1)*z2 + z1*(z3*z3) + (z2*z2)*z3 - z2*(z3*z3) - z1*(z2*z2) - z3*(z1*z1);
  
  if( fabs(det) < 1e-8 ){
    a = 0.0;
    b = 0.0;
    c = 0.0;
    return;
  }
  
  const float det1 = (x1)*z2 + z1*(x3) + (x2)*z3 - z2*(x3) - z1*(x2) - z3*(x1);
  const float det2 = (z1*z1)*x2 + x1*(z3*z3) + (z2*z2)*x3 - x2*(z3*z3) - x1*(z2*z2) - x3*(z1*z1);
  const float det3 = (z1*z1)*z2*x3 + z1*(z3*z3)*x2 + (z2*z2)*z3*x1 - z2*(z3*z3)*x1 - z1*(z2*z2)*x3 - z3*(z1*z1)*x2;

  a = det1/det;
  b = det2/det;
  c = det3/det;
  
  




}

