// Include files 
    2 
    3 // from Gaudi
    4 #include "GaudiKernel/AlgFactory.h"
    5 #include "Event/Track.h"
    6 #include "Event/StateParameters.h"
    7 // local
    8 #include "PrSeedingXLayers.h"
    9 #include "PrPlaneCounter.h"
   10 
   11 //-----------------------------------------------------------------------------
   12 // Implementation file for class : PrSeedingXLayers
   13 //
   14 // 2013-02-14 : Olivier Callot
   15 // 2013-03-21 : Yasmine Amhis Modification 
   16 //-----------------------------------------------------------------------------
   17 
   18 // Declaration of the Algorithm Factory
   19 DECLARE_ALGORITHM_FACTORY( PrSeedingXLayers )
   20 
   21 //=============================================================================
   22 // Standard constructor, initializes variables
   23 //=============================================================================
   24 PrSeedingXLayers::PrSeedingXLayers( const std::string& name,
   25                                         ISvcLocator* pSvcLocator)
   26 : GaudiAlgorithm ( name , pSvcLocator ),
   27   m_hitManager(nullptr),
   28   m_geoTool(nullptr),
   29   m_debugTool(nullptr),
   30   m_timerTool(nullptr)
   31 {
   32   declareProperty( "InputName",           m_inputName            = LHCb::TrackLocation::Forward );
   33   declareProperty( "OutputName",          m_outputName           = LHCb::TrackLocation::Seed    );
   34   declareProperty( "HitManagerName",      m_hitManagerName       = "PrFTHitManager"             );
   35   declareProperty( "DecodeData",          m_decodeData           = false                        );
   36   declareProperty( "XOnly",               m_xOnly                = false                        );
   37   
   38   declareProperty( "MaxChi2InTrack",      m_maxChi2InTrack       = 5.5                          );
   39   declareProperty( "TolXInf",             m_tolXInf              = 0.5 * Gaudi::Units::mm       );
   40   declareProperty( "TolXSup",             m_tolXSup              = 8.0 * Gaudi::Units::mm       );
   41   declareProperty( "MinXPlanes",          m_minXPlanes           = 5                            );
   42   declareProperty( "MaxChi2PerDoF",       m_maxChi2PerDoF        = 4.0                          );
   43   declareProperty( "MaxParabolaSeedHits", m_maxParabolaSeedHits  = 4                            );
   44   declareProperty( "TolTyOffset",         m_tolTyOffset          = 0.002                        );
   45   declareProperty( "TolTySlope",          m_tolTySlope           = 0.015                        );
   46   declareProperty( "MaxIpAtZero",         m_maxIpAtZero          = 5000.                        );
   47   
   48   // Parameters for debugging
   49   declareProperty( "DebugToolName",       m_debugToolName         = ""                          );
   50   declareProperty( "WantedKey",           m_wantedKey             = -100                        );
   51   declareProperty( "TimingMeasurement",   m_doTiming              = false                       );
   52   declareProperty( "PrintSettings",       m_printSettings         = false                       );
   53   
   54 }
   55 //=============================================================================
   56 // Destructor
   57 //=============================================================================
   58 PrSeedingXLayers::~PrSeedingXLayers() {}
   59 
   60 //=============================================================================
   61 // Initialization
   62 //=============================================================================
   63 StatusCode PrSeedingXLayers::initialize() {
   64   StatusCode sc = GaudiAlgorithm::initialize(); // must be executed first
   65   if ( sc.isFailure() ) return sc;  // error printed already by GaudiAlgorithm
   66 
   67   if ( msgLevel(MSG::DEBUG) ) debug() << "==> Initialize" << endmsg;
   68 
   69   m_hitManager = tool<PrHitManager>( m_hitManagerName );
   70   m_hitManager->buildGeometry();
   71   m_geoTool = tool<PrGeometryTool>("PrGeometryTool");
   72 
   73   m_debugTool   = 0;
   74   if ( "" != m_debugToolName ) {
   75     m_debugTool = tool<IPrDebugTool>( m_debugToolName );
   76   } else {
   77     m_wantedKey = -100;  // no debug
   78   }
   79 
   80   if ( m_doTiming) {
   81     m_timerTool = tool<ISequencerTimerTool>( "SequencerTimerTool/Timer", this );
   82     m_timeTotal   = m_timerTool->addTimer( "PrSeeding total" );
   83     m_timerTool->increaseIndent();
   84     m_timeFromForward = m_timerTool->addTimer( "Convert Forward" );
   85     m_timeXProjection = m_timerTool->addTimer( "X Projection" );
   86     m_timeStereo      = m_timerTool->addTimer( "Add stereo" );
   87     m_timeFinal       = m_timerTool->addTimer( "Convert tracks" );
   88     m_timerTool->decreaseIndent();
   89   }
   90 
   91   if( m_decodeData ) info() << "Will decode the FT clusters!" << endmsg;
   92 
   93   // -- Print the settings of this algorithm in a readable way
   94   if( m_printSettings){
   95     
   96     info() << "========================================"             << endmsg
   97            << " InputName            = " <<  m_inputName             << endmsg
   98            << " OutputName           = " <<  m_outputName            << endmsg
   99            << " HitManagerName       = " <<  m_hitManagerName        << endmsg
  100            << " DecodeData           = " <<  m_decodeData            << endmsg
  101            << " XOnly                = " <<  m_xOnly                 << endmsg
  102            << " MaxChi2InTrack       = " <<  m_maxChi2InTrack        << endmsg
  103            << " TolXInf              = " <<  m_tolXInf               << endmsg
  104            << " TolXSup              = " <<  m_tolXSup               << endmsg
  105            << " MinXPlanes           = " <<  m_minXPlanes            << endmsg
  106            << " MaxChi2PerDoF        = " <<  m_maxChi2PerDoF         << endmsg
  107            << " MaxParabolaSeedHits  = " <<  m_maxParabolaSeedHits   << endmsg
  108            << " TolTyOffset          = " <<  m_tolTyOffset           << endmsg
  109            << " TolTySlope           = " <<  m_tolTySlope            << endmsg
  110            << " MaxIpAtZero          = " <<  m_maxIpAtZero           << endmsg
  111            << " DebugToolName        = " <<  m_debugToolName         << endmsg
  112            << " WantedKey            = " <<  m_wantedKey             << endmsg
  113            << " TimingMeasurement    = " <<  m_doTiming              << endmsg
  114            << "========================================"             << endmsg;
  115   }
  116   
  117 
  118 
  119 
  120   return StatusCode::SUCCESS;
  121 }
  122 
  123 //=============================================================================
  124 // Main execution
  125 //=============================================================================
  126 StatusCode PrSeedingXLayers::execute() {
  127   //always () << "Welcome to quick and dirty fix from Olivier " << endmsg; 
  128   if ( msgLevel(MSG::DEBUG) ) debug() << "==> Execute" << endmsg;
  129   if ( m_doTiming ) {
  130     m_timerTool->start( m_timeTotal );
  131     m_timerTool->start( m_timeFromForward );
  132   }
  133 
  134   LHCb::Tracks* result = new LHCb::Tracks();
  135   put( result, m_outputName );
  136 
  137   // -- This is only needed if the seeding is the first algorithm using the FT
  138   // -- As the Forward normally runs first, it's off per default
  139   if( m_decodeData ) m_hitManager->decodeData();   
  140 
  141   for ( unsigned int zone = 0; m_hitManager->nbZones() > zone; ++zone ) {
  142     for ( PrHits::const_iterator itH = m_hitManager->hits( zone ).begin();
  143           m_hitManager->hits( zone ).end() != itH; ++itH ) {
  144       (*itH)->setUsed( false );
  145     }
  146   }
  147 
  148   //== If needed, debug the cluster associated to the requested MC particle.
  149   if ( 0 <= m_wantedKey ) {
  150     info() << "--- Looking for MCParticle " << m_wantedKey << endmsg;
  151     for ( unsigned int zone = 0; m_hitManager->nbZones() > zone; ++zone ) {
  152       for ( PrHits::const_iterator itH = m_hitManager->hits( zone ).begin();
  153             m_hitManager->hits( zone ).end() != itH; ++itH ) {
  154         if ( matchKey( *itH ) ) printHit( *itH, " " );
  155       }
  156     }
  157   }
  158   //====================================================================
  159   // Extract the seed part from the forward tracks.
  160   //====================================================================
  161   if ( "" != m_inputName ) {
  162     
  163     // -- sort hits according to LHCbID
  164     for(int i = 0; i < 24; i++){
  165       PrHitZone* zone = m_hitManager->zone(i);
  166       std::stable_sort( zone->hits().begin(),  zone->hits().end(), compLHCbID());
  167     }
  168     // ------------------------------------------
  169     
  170     LHCb::Tracks* forward = get<LHCb::Tracks>( m_inputName );
  171     for ( LHCb::Tracks::iterator itT = forward->begin(); forward->end() != itT; ++itT ) {
  172       std::vector<LHCb::LHCbID> ids;
  173       ids.reserve(20);
  174       for ( std::vector<LHCb::LHCbID>::const_iterator itId = (*itT)->lhcbIDs().begin();
  175             (*itT)->lhcbIDs().end() != itId; ++itId ) {
  176         if ( (*itId).isFT() ) {
  177           LHCb::FTChannelID ftId =(*itId).ftID();
  178           int zoneNb = 2 * ftId.layer() + ftId.mat(); //zones top are even (0, 2, 4, ....,22)  and zones bottom are odd 
  179           PrHitZone* zone = m_hitManager->zone(zoneNb);
  180           // -- The hits are sorted according to LHCbID, we can therefore use a lower bound to speed up the search
  181           PrHits::iterator itH = std::lower_bound(  zone->hits().begin(),  zone->hits().begin(), *itId, lowerBoundLHCbID() );
  182                     
  183           for ( ; zone->hits().end() != itH; ++itH ) {
  184             if( *itId < (*itH)->id() ) break;
  185             if ( (*itH)->id() == *itId ) (*itH)->setUsed( true );
  186           }
  187           ids.push_back( *itId );
  188         }
  189       }
  190       
  191       LHCb::Track* seed = new LHCb::Track;
  192       seed->setLhcbIDs( ids );
  193       seed->setType( LHCb::Track::Ttrack );
  194       seed->setHistory( LHCb::Track::PrSeeding );
  195       seed->setPatRecStatus( LHCb::Track::PatRecIDs );
  196       seed->addToStates( (*itT)->closestState( 9000. ) );
  197       result->insert( seed );
  198     }
  199 
  200     // -- sort hits according to x
  201     for(int i = 0; i < 24; i++){
  202       PrHitZone* zone = m_hitManager->zone(i);
  203       std::stable_sort( zone->hits().begin(),  zone->hits().end(), compX());
  204     }
  205 
  206 
  207 
  208   }
  209 
  210 
  211   m_trackCandidates.clear();
  212   if ( m_doTiming ) {
  213     m_timerTool->stop( m_timeFromForward );
  214   }
  215 
  216   // -- Loop through lower and upper half
  217   for ( unsigned int part= 0; 2 > part; ++part ) {
  218     if ( m_doTiming ) {
  219       m_timerTool->start( m_timeXProjection);
  220     }
  221     findXProjections2( part );
  222 
  223     if ( m_doTiming ) {
  224       m_timerTool->stop( m_timeXProjection);
  225       m_timerTool->start( m_timeStereo);
  226     }
  227 
  228     if ( ! m_xOnly ) addStereo2( part );
  229     if ( m_doTiming ) {
  230       m_timerTool->stop( m_timeStereo);
  231     }
  232   }
  233 
  234   if ( m_doTiming ) {
  235     m_timerTool->start( m_timeFinal);
  236   }
  237 
  238   makeLHCbTracks( result );
  239 
  240   if ( m_doTiming ) {
  241     m_timerTool->stop( m_timeFinal);
  242     float tot = m_timerTool->stop( m_timeTotal );
  243     info() << format( "                                            Time %8.3f ms", tot )
  244            << endmsg;
  245   }
  246 
  247   return StatusCode::SUCCESS;
  248 }
  249 
  250 //=============================================================================
  251 //  Finalize
  252 //=============================================================================
  253 StatusCode PrSeedingXLayers::finalize() {
  254 
  255   if ( msgLevel(MSG::DEBUG) ) debug() << "==> Finalize" << endmsg;
  256 
  257   return GaudiAlgorithm::finalize();  // must be called after all other actions
  258 }
  259 
  260 //=========================================================================
  261 //
  262 //=========================================================================
  263 void PrSeedingXLayers::findXProjections( unsigned int part ){
  264   m_xCandidates.clear();
  265   for ( unsigned int iCase = 0 ; 3 > iCase ; ++iCase ) {
  266     int firstZone = part;
  267     int lastZone  = 22 + part;
  268     if ( 1 == iCase ) firstZone = part + 6;
  269     if ( 2 == iCase ) lastZone  = 16 + part;
  270     
  271     PrHitZone* fZone = m_hitManager->zone( firstZone );
  272     PrHitZone* lZone = m_hitManager->zone( lastZone  );
  273     PrHits& fHits = fZone->hits();
  274     PrHits& lHits = lZone->hits();
  275     
  276         
  277     float zRatio =  lZone->z(0.) / fZone->z(0.);
  278     
  279   
  280 
  281     
  282     std::vector<PrHitZone*> xZones;
  283     xZones.reserve(12);
  284     for ( int kk = firstZone+2; lastZone > kk ; kk += 2 ) {
  285       if ( m_hitManager->zone( kk )->isX() ) xZones.push_back( m_hitManager->zone(kk) );
  286     }
  287     
  288     PrHits::iterator itLBeg = lHits.begin();
  289 
  290     // -- Define the iterators for the "in-between" layers
  291     std::vector< PrHits::iterator > iterators;
  292     iterators.reserve(24);
  293     
  294     for ( PrHits::iterator itF = fHits.begin(); fHits.end() != itF; ++itF ) {
  295       if ( 0 != iCase && (*itF)->isUsed() ) continue;
  296       float minXl = (*itF)->x() * zRatio - m_maxIpAtZero * ( zRatio - 1 );
  297       float maxXl = (*itF)->x() * zRatio + m_maxIpAtZero * ( zRatio - 1 );
  298   
  299 
  300       if ( matchKey( *itF ) ) info() << "Search from " << minXl << " to " << maxXl << endmsg;
  301       
  302       itLBeg = std::lower_bound( lHits.begin(), lHits.end(), minXl, lowerBoundX() );
  303       while ( itLBeg != lHits.end() && (*itLBeg)->x() < minXl ) {
  304         ++itLBeg;
  305         if ( lHits.end() == itLBeg ) break;
  306       }
  307 
  308       PrHits::iterator itL = itLBeg;
  309     
  310       // -- Initialize the iterators
  311       iterators.clear();
  312       for(std::vector<PrHitZone*>::iterator it = xZones.begin(); it != xZones.end(); it++){
  313         iterators.push_back( (*it)->hits().end() );
  314       }
  315       
  316 
  317       while ( itL != lHits.end() && (*itL)->x() < maxXl ) {
  318       
  319         
  320         if ( 0 != iCase && (*itL)->isUsed() ) {
  321           ++itL;
  322           continue;
  323         }
  324      
  325         
  326         float tx = ((*itL)->x() - (*itF)->x()) / (lZone->z() - fZone->z() );
  327         float x0 = (*itF)->x() - (*itF)->z() * tx;
  328         bool debug = matchKey( *itF ) || matchKey( *itL );
  329         PrHits xCandidate;
  330         xCandidate.reserve(30); // assume no more than 5 hits per layer
  331         xCandidate.push_back( *itF );
  332         int nMiss = 0;
  333         if ( 0 != iCase ) nMiss = 1;
  334         
  335 
  336         //loop over ALL x zones 
  337         unsigned int counter = 0; // counter to identify the iterators
  338         for ( std::vector<PrHitZone*>::iterator itZ = xZones.begin(); xZones.end() != itZ; ++itZ ) {
  339           
  340           float xP   = x0 + (*itZ)->z() * tx;
  341           float xMax = xP + m_tolXSup;
  342           float xMin = xP - m_tolXInf;
  343         
  344           if ( x0 < 0 ) {
  345             xMin = xP - m_tolXSup;
  346             xMax = xP + m_tolXInf;
  347           }
  348   
  349           // -- If iterator is not set to any position, search lower bound for it
  350           if(  iterators.at( counter ) == (*itZ)->hits().end() ){
  351             iterators.at( counter ) =  std::lower_bound( (*itZ)->hits().begin(), (*itZ)->hits().end(), xMin, lowerBoundX() );
  352           }
  353           PrHits::iterator itH = iterators.at( counter );
  354           
  355           float xMinStrict = xP - m_tolXSup;
  356           PrHit* best = nullptr;
  357 
  358           for ( ; (*itZ)->hits().end() != itH; ++itH ) {
  359             
  360             // -- Advance the iterator, if it is lower than any possible bound 
  361             //-- (as lower bound could change, need to use strict criterion here)
  362             if ( (*itH)->x() < xMinStrict ){
  363               iterators.at( counter )++;
  364               continue;
  365             }
  366             
  367             if ( (*itH)->x() < xMin ) continue;
  368             if ( (*itH)->x() > xMax ) break;
  369             
  370             best = *itH;
  371             xCandidate.push_back( best );
  372           }
  373           if ( nullptr == best ) {
  374             nMiss++;
  375             if ( 1 < nMiss ) break;
  376           }
  377           counter++;
  378  
  379         }
  380         
  381 
  382         if ( nMiss < 2 ) {
  383           xCandidate.push_back( *itL );
  384           PrSeedTrack temp( part, m_geoTool->zReference(), xCandidate );
  385           
  386           bool OK = fitTrack( temp );
  387           if ( debug ) {
  388             info() << "=== before fit === OK = " << OK << endmsg;
  389             printTrack( temp );
  390           }
  391           
  392           while ( !OK ) {
  393             OK = removeWorstAndRefit( temp );
  394           }
  395           setChi2( temp );
  396           if ( debug ) {
  397             info() << "=== after fit === chi2/dof = " << temp.chi2PerDoF() << endmsg;
  398             printTrack( temp );
  399           }
  400           if ( OK && 
  401                temp.hits().size() >= m_minXPlanes &&
  402                temp.chi2PerDoF()  < m_maxChi2PerDoF   ) {
  403             if ( temp.hits().size() == 6 ) {
  404               for ( PrHits::iterator itH = temp.hits().begin(); temp.hits().end() != itH; ++ itH) {
  405                 (*itH)->setUsed( true );
  406               }
  407             }
  408             
  409             m_xCandidates.push_back( temp );
  410             if ( debug ) {
  411               info() << "Candidate chi2PerDoF " << temp.chi2PerDoF() << endmsg;
  412               printTrack( temp );
  413             }
  414           } else {
  415             if ( debug ) info() << "OK " << OK << " size " << temp.hits().size() << " chi2 " << temp.chi2() << endmsg;
  416           }
  417         }
  418         ++itL;
  419       }
  420     }
  421   }
  422 
  423   std::stable_sort( m_xCandidates.begin(), m_xCandidates.end(), PrSeedTrack::GreaterBySize() );
  424 
  425   //====================================================================
  426   // Remove clones, i.e. share more than 2 hits
  427   //====================================================================
  428   for ( PrSeedTracks::iterator itT1 = m_xCandidates.begin(); m_xCandidates.end() !=itT1; ++itT1 ) {
  429     if ( !(*itT1).valid() ) continue;
  430     if ( (*itT1).hits().size() != 6 ) {
  431       int nUsed = 0;
  432       for ( PrHits::iterator itH = (*itT1).hits().begin(); (*itT1).hits().end() != itH; ++ itH) {
  433         if ( (*itH)->isUsed() ) ++nUsed;
  434       }
  435       if ( 2 < nUsed ) {
  436         (*itT1).setValid( false );
  437         continue;
  438       }
  439     }    
  440 
  441     for ( PrSeedTracks::iterator itT2 = itT1 + 1; m_xCandidates.end() !=itT2; ++itT2 ) {
  442       if ( !(*itT2).valid() ) continue;
  443       int nCommon = 0;
  444       PrHits::iterator itH1 = (*itT1).hits().begin();
  445       PrHits::iterator itH2 = (*itT2).hits().begin();
  446       while ( itH1 != (*itT1).hits().end() && itH2 != (*itT2).hits().end() ) {
  447         if ( (*itH1)->id() == (*itH2)->id() ) {
  448           nCommon++;
  449           ++itH1;
  450           ++itH2;
  451         } else if ( (*itH1)->id() < (*itH2)->id() ) {
  452           ++itH1;
  453         } else {
  454           ++itH2;
  455         }
  456       }
  457       if ( nCommon > 2 ) {
  458         if ( (*itT1).hits().size() > (*itT2).hits().size() ) {
  459           (*itT2).setValid( false );
  460         } else if ( (*itT1).hits().size() < (*itT2).hits().size() ) {
  461           (*itT1).setValid( false );
  462         } else if ( (*itT1).chi2PerDoF() < (*itT2).chi2PerDoF() ) {
  463           (*itT2).setValid( false );
  464         } else {
  465           (*itT1).setValid( false );
  466         }
  467       }
  468     }
  469     if ( m_xOnly ) m_trackCandidates.push_back( *itT1 );
  470   }
  471 }
  472 
  473 //=========================================================================
  474 // Add Stereo hits
  475 //=========================================================================
  476 void PrSeedingXLayers::addStereo( unsigned int part ) {
  477   PrSeedTracks xProjections;
  478   for ( PrSeedTracks::iterator itT1 = m_xCandidates.begin(); m_xCandidates.end() !=itT1; ++itT1 ) {
  479     if ( !(*itT1).valid() ) continue;
  480     xProjections.push_back( *itT1 );
  481   }
  482 
  483   unsigned int firstZone = part + 2;
  484   unsigned int lastZone  = part + 22;
  485   for ( PrSeedTracks::iterator itT = xProjections.begin(); xProjections.end() !=itT; ++itT ) {
  486     bool debug = false;
  487     int nMatch = 0;
  488     for ( PrHits::iterator itH = (*itT).hits().begin(); (*itT).hits().end() != itH; ++itH ) {
  489       if (  matchKey( *itH ) ) ++nMatch;
  490     }
  491     debug = nMatch > 3;
  492     if ( debug ) {
  493       info() << "Processing track " << endmsg;
  494       printTrack( *itT );
  495     }
  496     
  497     PrHits myStereo;
  498     myStereo.reserve(30);
  499     for ( unsigned int kk = firstZone; lastZone > kk ; kk+= 2 ) {
  500       if ( m_hitManager->zone(kk)->isX() ) continue;
  501       float dxDy = m_hitManager->zone(kk)->dxDy();
  502       float zPlane = m_hitManager->zone(kk)->z();
  503       
  504       float xPred = (*itT).x( m_hitManager->zone(kk)->z() );
  505       float xMin = xPred + 2700. * dxDy;
  506       float xMax = xPred - 2700. * dxDy;
  507       if ( xMin > xMax ) {
  508         float tmp = xMax;
  509         xMax = xMin;
  510         xMin = tmp;
  511       }
  512 
  513       PrHits::iterator itH = std::lower_bound( m_hitManager->zone(kk)->hits().begin(),  
  514                                                m_hitManager->zone(kk)->hits().end(), xMin, lowerBoundX() );
  515       //      for ( PrHits::iterator itH = m_hitManager->zone(kk)->hits().begin();
  516       for ( ;
  517             m_hitManager->zone(kk)->hits().end() != itH; ++itH ) {
  518         if ( (*itH)->x() < xMin ) continue;
  519         if ( (*itH)->x() > xMax ) break;
  520         (*itH)->setCoord( ((*itH)->x() - xPred) / dxDy / zPlane );
  521         if ( debug ) {
  522           if ( matchKey( *itH ) ) printHit( *itH, "" );
  523         }
  524         if ( 0 == part && (*itH)->coord() < -0.005 ) continue;
  525         if ( 1 == part && (*itH)->coord() >  0.005 ) continue;
  526         myStereo.push_back( *itH );
  527       }
  528     }
  529     std::stable_sort( myStereo.begin(), myStereo.end(), PrHit::LowerByCoord() );
  530 
  531     PrPlaneCounter plCount;
  532     unsigned int firstSpace = m_trackCandidates.size();
  533 
  534     PrHits::iterator itBeg = myStereo.begin();
  535     PrHits::iterator itEnd = itBeg + 5;
  536     
  537     while ( itEnd < myStereo.end() ) {
  538       float tolTy = 0.002 + .02 * fabs( (*itBeg)->coord() );
  539       if ( (*(itEnd-1))->coord() - (*itBeg)->coord() < tolTy ) {
  540         while( itEnd+1 < myStereo.end() &&
  541                (*itEnd)->coord() - (*itBeg)->coord() < tolTy ) {
  542           ++itEnd;
  543         }
  544         plCount.set( itBeg, itEnd );
  545         if ( 4 < plCount.nbDifferent() ) {
  546           PrSeedTrack temp( *itT );
  547           for ( PrHits::iterator itH = itBeg; itEnd != itH; ++itH ) temp.addHit( *itH );
  548           bool ok = fitTrack( temp );
  549           ok = fitTrack( temp );
  550           ok = fitTrack( temp );
  551           if ( debug ) {
  552             info() << "Before fit-and-remove" << endmsg;
  553             printTrack( temp );
  554           }
  555           while ( !ok && temp.hits().size() > 10 ) {
  556             ok = removeWorstAndRefit( temp );
  557             if ( debug ) {
  558               info() << "    after removing worse" << endmsg;
  559               printTrack( temp );
  560             }
  561           }
  562           if ( ok ) {
  563             setChi2( temp );
  564             if ( debug ) {
  565               info() << "=== Candidate chi2 " << temp.chi2PerDoF() << endmsg;
  566               printTrack( temp );
  567             }
  568             if ( temp.hits().size() > 9 || 
  569                  temp.chi2PerDoF() < m_maxChi2PerDoF ) {
  570               m_trackCandidates.push_back( temp );
  571             }
  572             itBeg += 4;
  573           }
  574         }
  575       }
  576       ++itBeg;
  577       itEnd = itBeg + 5;
  578     }
  579     //=== Remove bad candidates: Keep the best for this input track
  580     if ( m_trackCandidates.size() > firstSpace+1 ) {
  581       for ( unsigned int kk = firstSpace; m_trackCandidates.size()-1 > kk ; ++kk ) {
  582         if ( !m_trackCandidates[kk].valid() ) continue;
  583         for ( unsigned int ll = kk + 1; m_trackCandidates.size() > ll; ++ll ) {
  584           if ( !m_trackCandidates[ll].valid() ) continue;
  585           if ( m_trackCandidates[ll].hits().size() < m_trackCandidates[kk].hits().size() ) {
  586             m_trackCandidates[ll].setValid( false );
  587           } else if ( m_trackCandidates[ll].hits().size() > m_trackCandidates[kk].hits().size() ) {
  588             m_trackCandidates[kk].setValid( false );
  589           } else if ( m_trackCandidates[kk].chi2() < m_trackCandidates[ll].chi2() ) {
  590             m_trackCandidates[ll].setValid( false );
  591           } else {
  592             m_trackCandidates[kk].setValid( false );
  593           }  
  594         }   
  595       }
  596     }
  597   }
  598 }
  599 
  600 //=========================================================================
  601 //  Fit the track, return OK if fit sucecssfull
  602 //=========================================================================
  603 bool PrSeedingXLayers::fitTrack( PrSeedTrack& track ) {
  604 
  605   for ( int loop = 0; 3 > loop ; ++loop ) {
  606     //== Fit a parabola
  607     float s0   = 0.;
  608     float sz   = 0.;
  609     float sz2  = 0.;
  610     float sz3  = 0.;
  611     float sz4  = 0.;
  612     float sd   = 0.;
  613     float sdz  = 0.;
  614     float sdz2 = 0.;
  615     float sdz3 = 0.;
  616 
  617     float t0  = 0.;
  618     float tz  = 0.;
  619     float tz2 = 0.;
  620     float td  = 0.;
  621     float tdz = 0.;
  622 
  623     for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
  624       float w = (*itH)->w();
  625       float z = (*itH)->z() - m_geoTool->zReference();
  626       if ( (*itH)->dxDy() != 0 ) {
  627         if ( 0 == loop ) continue;
  628         float dy = track.deltaY( *itH );
  629         t0   += w;
  630         tz   += w * z;
  631         tz2  += w * z * z;
  632         td   += w * dy;
  633         tdz  += w * dy * z;
  634       }
  635       float d = track.distance( *itH );
  636       s0   += w;
  637       sz   += w * z;
  638       sz2  += w * z * z;
  639       sz3  += w * z * z * z;
  640       sz4  += w * z * z * z * z;
  641       sd   += w * d;
  642       sdz  += w * d * z;
  643       sdz2 += w * d * z * z;
  644       sdz3 += w * d * z * z;
  645     }
  646     float b1 = sz  * sz  - s0  * sz2;
  647     float c1 = sz2 * sz  - s0  * sz3;
  648     float d1 = sd  * sz  - s0  * sdz;
  649     float b2 = sz2 * sz2 - sz * sz3;
  650     float c2 = sz3 * sz2 - sz * sz4;
  651     float d2 = sdz * sz2 - sz * sdz2;
  652 
  653     float den = (b1 * c2 - b2 * c1 );
  654     if( fabs(den) < 1e-9 ) return false;
  655     float db  = (d1 * c2 - d2 * c1 ) / den;
  656     float dc  = (d2 * b1 - d1 * b2 ) / den;
  657     float da  = ( sd - db * sz - dc * sz2 ) / s0;
  658 
  659     float day = 0.;
  660     float dby = 0.;
  661     if ( t0 > 0. ) {
  662       float deny = (tz  * tz - t0 * tz2);
  663       day = -(tdz * tz - td * tz2) / deny;
  664       dby = -(td  * tz - t0 * tdz) / deny;
  665     }
  666 
  667     track.updateParameters( da, db, dc, day, dby );
  668     float maxChi2 = 0.;
  669     for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
  670       float chi2 = track.chi2( *itH );
  671       if ( chi2 > maxChi2 ) {
  672         maxChi2 = chi2;
  673       }
  674     }
  675     if ( m_maxChi2InTrack > maxChi2 ) return true;
  676   }
  677   return false;
  678 }
  679 
  680 //=========================================================================
  681 //  Remove the worst hit and refit.
  682 //=========================================================================
  683 bool PrSeedingXLayers::removeWorstAndRefit ( PrSeedTrack& track ) {
  684   float maxChi2 = 0.;
  685   PrHits::iterator worst = track.hits().begin();
  686   for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
  687     float chi2 = track.chi2( *itH );
  688     if ( chi2 > maxChi2 ) {
  689       maxChi2 = chi2;
  690       worst = itH;
  691     }
  692   }
  693   track.hits().erase( worst );
  694   return fitTrack( track );
  695 }
  696 //=========================================================================
  697 //  Set the chi2 of the track
  698 //=========================================================================
  699 void PrSeedingXLayers::setChi2 ( PrSeedTrack& track ) {
  700   float chi2 = 0.;
  701   int   nDoF = -3;  // Fitted a parabola
  702   bool hasStereo = false;
  703   for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
  704     float d = track.distance( *itH );
  705     if ( (*itH)->dxDy() != 0 ) hasStereo = true;
  706     float w = (*itH)->w();
  707     chi2 += w * d * d;
  708     nDoF += 1;
  709   }
  710   if ( hasStereo ) nDoF -= 2;
  711   track.setChi2( chi2, nDoF );
  712 }
  713 
  714 //=========================================================================
  715 //  Convert to LHCb tracks
  716 //=========================================================================
  717 void PrSeedingXLayers::makeLHCbTracks ( LHCb::Tracks* result ) {
  718   for ( PrSeedTracks::iterator itT = m_trackCandidates.begin();
  719         m_trackCandidates.end() != itT; ++itT ) {
  720     if ( !(*itT).valid() ) continue;
  721 
  722     //info() << "==== Store track ==== chi2/dof " << (*itT).chi2PerDoF() << endmsg;
  723     //printTrack( *itT );
  724     
  725     LHCb::Track* tmp = new LHCb::Track;
  726     //tmp->setType( LHCb::Track::Long );
  727     //tmp->setHistory( LHCb::Track::PatForward );
  728     tmp->setType( LHCb::Track::Ttrack );
  729     tmp->setHistory( LHCb::Track::PrSeeding );
  730     double qOverP = m_geoTool->qOverP( *itT );
  731 
  732     LHCb::State tState;
  733     double z = StateParameters::ZEndT;
  734     tState.setLocation( LHCb::State::AtT );
  735     tState.setState( (*itT).x( z ), (*itT).y( z ), z, (*itT).xSlope( z ), (*itT).ySlope( ), qOverP );
  736 
  737     //== overestimated covariance matrix, as input to the Kalman fit
  738 
  739     tState.setCovariance( m_geoTool->covariance( qOverP ) );
  740     tmp->addToStates( tState );
  741 
  742     //== LHCb ids.
  743 
  744     tmp->setPatRecStatus( LHCb::Track::PatRecIDs );
  745     for ( PrHits::iterator itH = (*itT).hits().begin(); (*itT).hits().end() != itH; ++itH ) {
  746       tmp->addToLhcbIDs( (*itH)->id() );
  747     }
  748     tmp->setChi2PerDoF( (*itT).chi2PerDoF() );
  749     tmp->setNDoF(       (*itT).nDoF() );
  750     result->insert( tmp );
  751   }
  752 }
  753 
  754 //=========================================================================
  755 //  Print the information of the selected hit
  756 //=========================================================================
  757 void PrSeedingXLayers::printHit ( const PrHit* hit, std::string title ) {
  758   info() << "  " << title
  759          << format( "Plane%3d zone%2d z0 %8.2f x0 %8.2f  size%2d charge%3d coord %8.3f used%2d ",
  760                     hit->planeCode(), hit->zone(), hit->z(), hit->x(),
  761                     hit->size(), hit->charge(), hit->coord(), hit->isUsed() );
  762   if ( m_debugTool ) m_debugTool->printKey( info(), hit->id() );
  763   if ( matchKey( hit ) ) info() << " ***";
  764   info() << endmsg;
  765 }
  766 
  767 //=========================================================================
  768 //  Print the whole track
  769 //=========================================================================
  770 void PrSeedingXLayers::printTrack ( PrSeedTrack& track ) {
  771   for ( PrHits::iterator itH = track.hits().begin(); track.hits().end() != itH; ++itH ) {
  772     info() << format( "dist %7.3f dy %7.2f chi2 %7.2f ", track.distance( *itH ), track.deltaY( *itH ), track.chi2( *itH ) );
  773     printHit( *itH );
  774   }
  775 }
  776 //=========================================================================
  777 // modified method to find the x projections
  778 //=========================================================================
  779 void PrSeedingXLayers::findXProjections2( unsigned int part ){
  780   m_xCandidates.clear();
  781   for ( unsigned int iCase = 0 ; 3 > iCase ; ++iCase ) {
  782     int firstZone = part;
  783     int lastZone  = 22 + part;
  784     if ( 1 == iCase ) firstZone = part + 6;
  785     if ( 2 == iCase ) lastZone  = 16 + part;
  786     
  787     PrHitZone* fZone = m_hitManager->zone( firstZone );
  788     PrHitZone* lZone = m_hitManager->zone( lastZone  );
  789     PrHits& fHits = fZone->hits();
  790     PrHits& lHits = lZone->hits();
  791     
  792         
  793     float zRatio =  lZone->z(0.) / fZone->z(0.);
  794     
  795   
  796 
  797     
  798     std::vector<PrHitZone*> xZones;
  799     xZones.reserve(12);
  800     for ( int kk = firstZone+2; lastZone > kk ; kk += 2 ) {
  801       if ( m_hitManager->zone( kk )->isX() ) xZones.push_back( m_hitManager->zone(kk) );
  802     }
  803     
  804     PrHits::iterator itLBeg = lHits.begin();
  805 
  806     // -- Define the iterators for the "in-between" layers
  807     std::vector< PrHits::iterator > iterators;
  808     iterators.reserve(24);
  809     
  810     for ( PrHits::iterator itF = fHits.begin(); fHits.end() != itF; ++itF ) {
  811       if ( 0 != iCase && (*itF)->isUsed() ) continue;
  812       float minXl = (*itF)->x() * zRatio - m_maxIpAtZero * ( zRatio - 1 );
  813       float maxXl = (*itF)->x() * zRatio + m_maxIpAtZero * ( zRatio - 1 );
  814   
  815 
  816       if ( matchKey( *itF ) ) info() << "Search from " << minXl << " to " << maxXl << endmsg;
  817       
  818       itLBeg = std::lower_bound( lHits.begin(), lHits.end(), minXl, lowerBoundX() );
  819       while ( itLBeg != lHits.end() && (*itLBeg)->x() < minXl ) {
  820         ++itLBeg;
  821         if ( lHits.end() == itLBeg ) break;
  822       }
  823 
  824       PrHits::iterator itL = itLBeg;
  825     
  826       while ( itL != lHits.end() && (*itL)->x() < maxXl ) {
  827       
  828         
  829         if ( 0 != iCase && (*itL)->isUsed()) {
  830           ++itL;
  831           continue;
  832         }
  833      
  834         
  835         float tx = ((*itL)->x() - (*itF)->x()) / (lZone->z() - fZone->z() );
  836         float x0 = (*itF)->x() - (*itF)->z() * tx;
  837       
  838         PrHits parabolaSeedHits;
  839         parabolaSeedHits.clear();
  840         parabolaSeedHits.reserve(5);
  841         
  842         // -- loop over first two x zones
  843         // --------------------------------------------------------------------------------
  844         unsigned int counter = 0; 
  845 
  846         bool skip = true;
  847         if( iCase != 0 ) skip = false;
  848         
  849         
  850         for ( std::vector<PrHitZone*>::iterator itZ = xZones.begin(); xZones.end() != itZ; ++itZ ) {
  851         
  852           ++counter;
  853 
  854           // -- to make sure, in case = 0, only x layers of the 2nd T station are used
  855           if(skip){
  856             skip = false;
  857             continue;
  858           }
  859           
  860           
  861           if( iCase == 0){
  862             if(counter > 3) break;
  863           }else{
  864             if(counter > 2) break;
  865           }
  866           
  867           
  868           
  869           float xP   = x0 + (*itZ)->z() * tx;
  870           float xMax = xP + 2*fabs(tx)*m_tolXSup + 1.5;
  871           float xMin = xP - m_tolXInf;
  872         
  873           if ( x0 < 0 ) {
  874             xMin = xP - 2*fabs(tx)*m_tolXSup - 1.5;
  875             xMax = xP + m_tolXInf;
  876           }
  877          
  878           PrHits::iterator itH = std::lower_bound( (*itZ)->hits().begin(), (*itZ)->hits().end(), xMin, lowerBoundX() );
  879           for ( ; (*itZ)->hits().end() != itH; ++itH ) {
  880             
  881             if ( (*itH)->x() < xMin ) continue;
  882             if ( (*itH)->x() > xMax ) break;
  883             
  884             parabolaSeedHits.push_back( *itH );
  885           }
  886         }
  887         // --------------------------------------------------------------------------------
  888 
  889         debug() << "We have " << parabolaSeedHits.size() << " hits to seed the parabolas" << endmsg;
  890 
  891         std::vector<PrHits> xHitsLists;
  892         xHitsLists.clear();
  893 
  894 
  895         // -- float xP   = x0 + (*itZ)->z() * tx;
  896         // -- Alles klar, Herr Kommissar?
  897         // -- The power of Lambda functions!
  898         // -- Idea is to reduce ghosts in very busy events and prefer the high momentum tracks
  899         // -- For this, the seedHits are storted according to their distance to the linear extrapolation
  900         // -- so that the ones with the least distance can be chosen in the end
  901         std::stable_sort( parabolaSeedHits.begin(), parabolaSeedHits.end(), 
  902                    [x0,tx](const PrHit* lhs, const PrHit* rhs)
  903                    ->bool{return fabs(lhs->x() - (x0+lhs->z()*tx)) <  fabs(rhs->x() - (x0+rhs->z()*tx)); });
  904         
  905         
  906         unsigned int maxParabolaSeedHits = m_maxParabolaSeedHits;
  907         if( parabolaSeedHits.size() < m_maxParabolaSeedHits){
  908           maxParabolaSeedHits = parabolaSeedHits.size();
  909         }
  910         
  911 
  912         for(unsigned int i = 0; i < maxParabolaSeedHits; ++i){
  913           
  914           float a = 0;
  915           float b = 0;
  916           float c = 0;
  917           
  918           PrHits xHits;
  919           xHits.clear();
  920           
  921           
  922           // -- formula is: x = a*dz*dz + b*dz + c = x, with dz = z - zRef
  923           solveParabola( *itF, parabolaSeedHits[i], *itL, a, b, c);
  924           
  925           debug() << "parabola equation: x = " << a << "*z^2 + " << b << "*z + " << c << endmsg;
  926           
  927 
  928           for ( std::vector<PrHitZone*>::iterator itZ = xZones.begin(); xZones.end() != itZ; ++itZ ) {
  929           
  930             float dz = (*itZ)->z() - m_geoTool->zReference();
  931             float xAtZ = a*dz*dz + b*dz + c;
  932             
  933             float xP   = x0 + (*itZ)->z() * tx;
  934             float xMax = xAtZ + fabs(tx)*2.0 + 0.5;
  935             float xMin = xAtZ - fabs(tx)*2.0 - 0.5;
  936                         
  937             
  938             debug() << "x prediction (linear): " << xP <<  "x prediction (parabola): " << xAtZ << endmsg;
  939             
  940             
  941             // -- Only use one hit per layer, which is closest to the parabola!
  942             PrHit* best = nullptr;
  943             float bestDist = 10.0;
  944             
  945             
  946             PrHits::iterator itH = std::lower_bound( (*itZ)->hits().begin(), (*itZ)->hits().end(), xMin, lowerBoundX() );
  947             for (; (*itZ)->hits().end() != itH; ++itH ) {
  948               
  949               if ( (*itH)->x() < xMin ) continue;
  950               if ( (*itH)->x() > xMax ) break;
  951               
  952               
  953               if( fabs((*itH)->x() - xAtZ ) < bestDist){
  954                 bestDist = fabs((*itH)->x() - xAtZ );
  955                 best = *itH;
  956               }
  957               
  958             }
  959             if( best != nullptr) xHits.push_back( best );
  960           }
  961           
  962           xHits.push_back( *itF );
  963           xHits.push_back( *itL );
  964           
  965 
  966           if( xHits.size() < 5) continue;
  967           std::stable_sort(xHits.begin(), xHits.end(), compX());
  968        
  969           
  970           bool isEqual = false;
  971           
  972           for( PrHits hits : xHitsLists){
  973             if( hits == xHits ){
  974               isEqual = true;
  975               break;
  976             }
  977           }
  978           
  979 
  980           if( !isEqual ) xHitsLists.push_back( xHits );
  981         }
  982         
  983         
  984         debug() << "xHitsLists size before removing duplicates: " << xHitsLists.size() << endmsg;
  985         
  986         // -- remove duplicates
  987         
  988         if( xHitsLists.size() > 2){
  989           std::stable_sort( xHitsLists.begin(), xHitsLists.end() );
  990           xHitsLists.erase( std::unique(xHitsLists.begin(), xHitsLists.end()), xHitsLists.end());
  991         }
  992         
  993         debug() << "xHitsLists size after removing duplicates: " << xHitsLists.size() << endmsg;
  994         
  995 
  996         
  997         for( PrHits xHits : xHitsLists ){
  998           
  999           PrSeedTrack temp( part, m_geoTool->zReference(), xHits );
 1000           
 1001           bool OK = fitTrack( temp );
 1002           
 1003           while ( !OK ) {
 1004             OK = removeWorstAndRefit( temp );
 1005           }
 1006           setChi2( temp );
 1007           // ---------------------------------------
 1008   
 1009           float maxChi2 = m_maxChi2PerDoF + 6*tx*tx;
 1010           
 1011 
 1012           if ( OK && 
 1013                temp.hits().size() >= m_minXPlanes &&
 1014                temp.chi2PerDoF()  < maxChi2   ) {
 1015             if ( temp.hits().size() == 6 ) {
 1016               for ( PrHits::iterator itH = temp.hits().begin(); temp.hits().end() != itH; ++ itH) {
 1017                 (*itH)->setUsed( true );
 1018               }
 1019               
 1020               
 1021             }
 1022             
 1023             m_xCandidates.push_back( temp );
 1024           }
 1025           // -------------------------------------
 1026         }
 1027         ++itL;
 1028       }
 1029     }
 1030   }
 1031   
 1032   
 1033   std::stable_sort( m_xCandidates.begin(), m_xCandidates.end(), PrSeedTrack::GreaterBySize() );
 1034 
 1035   //====================================================================
 1036   // Remove clones, i.e. share more than 2 hits
 1037   //====================================================================
 1038   for ( PrSeedTracks::iterator itT1 = m_xCandidates.begin(); m_xCandidates.end() !=itT1; ++itT1 ) {
 1039     if ( !(*itT1).valid() ) continue;
 1040     if ( (*itT1).hits().size() != 6 ) {
 1041       int nUsed = 0;
 1042       for ( PrHits::iterator itH = (*itT1).hits().begin(); (*itT1).hits().end() != itH; ++ itH) {
 1043         if ( (*itH)->isUsed()) ++nUsed;
 1044       }
 1045       if ( 1 < nUsed ) {
 1046         (*itT1).setValid( false );
 1047         continue;
 1048       }
 1049     }    
 1050     
 1051     for ( PrSeedTracks::iterator itT2 = itT1 + 1; m_xCandidates.end() !=itT2; ++itT2 ) {
 1052       if ( !(*itT2).valid() ) continue;
 1053       int nCommon = 0;
 1054       PrHits::iterator itH1 = (*itT1).hits().begin();
 1055       PrHits::iterator itH2 = (*itT2).hits().begin();
 1056       
 1057       PrHits::iterator itEnd1 = (*itT1).hits().end();
 1058       PrHits::iterator itEnd2 = (*itT2).hits().end();
 1059 
 1060       while ( itH1 != itEnd1 && itH2 != itEnd2 ) {
 1061         if ( (*itH1)->id() == (*itH2)->id() ) {
 1062           ++nCommon;
 1063           ++itH1;
 1064           ++itH2;
 1065         } else if ( (*itH1)->id() < (*itH2)->id() ) {
 1066           ++itH1;
 1067         } else {
 1068           ++itH2;
 1069         }
 1070       }
 1071       if ( nCommon > 2 ) {
 1072         if ( (*itT1).hits().size() > (*itT2).hits().size() ) {
 1073           (*itT2).setValid( false );
 1074         } else if ( (*itT1).hits().size() < (*itT2).hits().size() ) {
 1075           (*itT1).setValid( false );
 1076         } else if ( (*itT1).chi2PerDoF() < (*itT2).chi2PerDoF() ) {
 1077           (*itT2).setValid( false );
 1078         } else {
 1079           (*itT1).setValid( false );
 1080         }
 1081       }
 1082     }
 1083     if ( m_xOnly ) m_trackCandidates.push_back( *itT1 );
 1084   }
 1085 }
 1086 //=========================================================================
 1087 // Modified version of adding the stereo layers
 1088 //=========================================================================
 1089 void PrSeedingXLayers::addStereo2( unsigned int part ) {
 1090   PrSeedTracks xProjections;
 1091   for ( PrSeedTracks::iterator itT1 = m_xCandidates.begin(); m_xCandidates.end() !=itT1; ++itT1 ) {
 1092     if ( !(*itT1).valid() ) continue;
 1093     xProjections.push_back( *itT1 );
 1094   }
 1095 
 1096   unsigned int firstZone = part + 2;
 1097   unsigned int lastZone  = part + 22;
 1098   for ( PrSeedTracks::iterator itT = xProjections.begin(); xProjections.end() !=itT; ++itT ) {
 1099     
 1100     PrHits myStereo;
 1101     myStereo.reserve(30);
 1102     for ( unsigned int kk = firstZone; lastZone > kk ; kk+= 2 ) {
 1103       if ( m_hitManager->zone(kk)->isX() ) continue;
 1104       float dxDy = m_hitManager->zone(kk)->dxDy();
 1105       float zPlane = m_hitManager->zone(kk)->z();
 1106 
 1107       float xPred = (*itT).x( m_hitManager->zone(kk)->z() );
 1108 
 1109       float xMin = xPred + 2500. * dxDy;
 1110       float xMax = xPred - 2500. * dxDy;
 1111       
 1112       if ( xMin > xMax ) {
 1113         float tmp = xMax;
 1114         xMax = xMin;
 1115         xMin = tmp;
 1116       }
 1117       
 1118 
 1119       PrHits::iterator itH = std::lower_bound( m_hitManager->zone(kk)->hits().begin(),  
 1120                                                m_hitManager->zone(kk)->hits().end(), xMin, lowerBoundX() );
 1121       
 1122       for ( ;
 1123             m_hitManager->zone(kk)->hits().end() != itH; ++itH ) {
 1124         
 1125         if ( (*itH)->x() < xMin ) continue;
 1126         if ( (*itH)->x() > xMax ) break;
 1127       
 1128         (*itH)->setCoord( ((*itH)->x() - xPred) / dxDy  / zPlane );
 1129         
 1130         if ( 1 == part && (*itH)->coord() < -0.005 ) continue;
 1131         if ( 0 == part && (*itH)->coord() >  0.005 ) continue;
 1132 
 1133         myStereo.push_back( *itH );
 1134       }
 1135     }
 1136     std::stable_sort( myStereo.begin(), myStereo.end(), PrHit::LowerByCoord() );
 1137 
 1138     PrPlaneCounter plCount;
 1139     unsigned int firstSpace = m_trackCandidates.size();
 1140 
 1141     PrHits::iterator itBeg = myStereo.begin();
 1142     PrHits::iterator itEnd = itBeg + 5;
 1143     
 1144     while ( itEnd < myStereo.end() ) {
 1145     
 1146       float tolTy = m_tolTyOffset + m_tolTySlope * fabs( (*itBeg)->coord() );
 1147 
 1148         if ( (*(itEnd-1))->coord() - (*itBeg)->coord() < tolTy ) {
 1149           while( itEnd+1 < myStereo.end() &&
 1150                  (*itEnd)->coord() - (*itBeg)->coord() < tolTy ) {
 1151             ++itEnd;
 1152           }
 1153           
 1154         
 1155           plCount.set( itBeg, itEnd );
 1156           if ( 4 < plCount.nbDifferent() ) {
 1157             PrSeedTrack temp( *itT );
 1158             for ( PrHits::iterator itH = itBeg; itEnd != itH; ++itH ) temp.addHit( *itH );
 1159             bool ok = fitTrack( temp );
 1160             ok = fitTrack( temp );
 1161             ok = fitTrack( temp );
 1162             
 1163         
 1164             while ( !ok && temp.hits().size() > 10 ) {
 1165               ok = removeWorstAndRefit( temp );
 1166             }
 1167             if ( ok ) {
 1168               setChi2( temp );
 1169               
 1170               float maxChi2 = m_maxChi2PerDoF + 6*temp.xSlope(9000)*temp.xSlope(9000);
 1171 
 1172               if ( temp.hits().size() > 9 || 
 1173                    temp.chi2PerDoF() < maxChi2 ) {
 1174                 m_trackCandidates.push_back( temp );
 1175                 
 1176               }
 1177               itBeg += 4;
 1178             }
 1179           }
 1180         }
 1181         ++itBeg;
 1182         itEnd = itBeg + 5;
 1183     }
 1184     
 1185 
 1186     
 1187     //=== Remove bad candidates: Keep the best for this input track
 1188     if ( m_trackCandidates.size() > firstSpace+1 ) {
 1189       for ( unsigned int kk = firstSpace; m_trackCandidates.size()-1 > kk ; ++kk ) {
 1190         if ( !m_trackCandidates[kk].valid() ) continue;
 1191         for ( unsigned int ll = kk + 1; m_trackCandidates.size() > ll; ++ll ) {
 1192           if ( !m_trackCandidates[ll].valid() ) continue;
 1193           if ( m_trackCandidates[ll].hits().size() < m_trackCandidates[kk].hits().size() ) {
 1194             m_trackCandidates[ll].setValid( false );
 1195           } else if ( m_trackCandidates[ll].hits().size() > m_trackCandidates[kk].hits().size() ) {
 1196             m_trackCandidates[kk].setValid( false );
 1197           } else if ( m_trackCandidates[kk].chi2() < m_trackCandidates[ll].chi2() ) {
 1198             m_trackCandidates[ll].setValid( false );
 1199           } else {
 1200             m_trackCandidates[kk].setValid( false );
 1201           }  
 1202         }   
 1203       }
 1204     }
 1205     
 1206   }
 1207 }
 1208 
 1209 
 1210 //=========================================================================
 1211 // Solve parabola using Cramer's rule 
 1212 //========================================================================
 1213 void PrSeedingXLayers::solveParabola(const PrHit* hit1, const PrHit* hit2, const PrHit* hit3, float& a, float& b, float& c){
 1214   
 1215   const float z1 = hit1->z() - m_geoTool->zReference();
 1216   const float z2 = hit2->z() - m_geoTool->zReference();
 1217   const float z3 = hit3->z() - m_geoTool->zReference();
 1218   
 1219   const float x1 = hit1->x();
 1220   const float x2 = hit2->x();
 1221   const float x3 = hit3->x();
 1222   
 1223   const float det = (z1*z1)*z2 + z1*(z3*z3) + (z2*z2)*z3 - z2*(z3*z3) - z1*(z2*z2) - z3*(z1*z1);
 1224   
 1225   if( fabs(det) < 1e-8 ){
 1226     a = 0.0;
 1227     b = 0.0;
 1228     c = 0.0;
 1229     return;
 1230   }
 1231   
 1232   const float det1 = (x1)*z2 + z1*(x3) + (x2)*z3 - z2*(x3) - z1*(x2) - z3*(x1);
 1233   const float det2 = (z1*z1)*x2 + x1*(z3*z3) + (z2*z2)*x3 - x2*(z3*z3) - x1*(z2*z2) - x3*(z1*z1);
 1234   const float det3 = (z1*z1)*z2*x3 + z1*(z3*z3)*x2 + (z2*z2)*z3*x1 - z2*(z3*z3)*x1 - z1*(z2*z2)*x3 - z3*(z1*z1)*x2;
 1235 
 1236   a = det1/det;
 1237   b = det2/det;
 1238   c = det3/det;
 1239   
 1240   
 1241 
 1242 
 1243 
 1244 
 1245 }
 1246 

