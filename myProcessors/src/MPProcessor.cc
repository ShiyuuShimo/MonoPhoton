#include "MPProcessor.h"
#include <iostream>

#include <EVENT/LCCollection.h>
#include <EVENT/MCParticle.h>
#include <EVENT/ReconstructedParticle.h>

// ----- include for verbosity dependend logging ---------
#include "marlin/VerbosityLevels.h"

#ifdef MARLIN_USE_AIDA
#include <marlin/AIDAProcessor.h>
#include <AIDA/IHistogramFactory.h>
#include <AIDA/ICloud1D.h>

//#include <AIDA/IHistogram1D.h>
#endif // MARLIN_USE_AIDA

#include "TFile.h"
#include "TTree.h"
#include "TMath.h"
#include "TVector3.h"

using namespace lcio ;
using namespace marlin ;


MPProcessor aMPProcessor ;


MPProcessor::MPProcessor() : Processor("MPProcessor") {
    
    // modify processor description
    _description = "MPProcessor does whatever it does ..." ;
    
    
    // register steering parameters: name, description, class-variable, default value
    registerInputCollection( LCIO::MCPARTICLE,
                            "InputMCParticleCollection" ,
                            "Name of the MCParticle collection"  ,
                            _colMCP ,
                            std::string("MCParticle")
                            );
    
    registerInputCollection( LCIO::RECONSTRUCTEDPARTICLE,
                            "InputPFOCollection" ,
                            "Name of the PFOs collection"  ,
                            _colPFO ,
                            std::string("PandoraPFOs")
                            ) ;
    
    registerInputCollection( LCIO::LCRELATION,
                            "InputMCTruthLinkCollection" ,
                            "Name of the MCTruthLink collection"  ,
                            _colMCPFORelation ,
                            std::string("RecoMCTruthLink")
                            ) ;
    
    registerProcessorParameter( "OutputRootFileName",
                               "Name of Root file (default: output.root)",
                               _rootfilename,
                               std::string("output.root")
                               );
    
}



void MPProcessor::init() { 
    
    streamlog_out(DEBUG) << "   init called  " << std::endl ;
    
    // usually a good idea to
    printParameters() ;
    
    _nRun = 0 ;
    _nEvt = 0 ;
    
    makeNTuple();
    
}


void MPProcessor::processRunHeader( LCRunHeader* run) { 
    
    _nRun++ ;
} 



void MPProcessor::processEvent( LCEvent * evt ) { 
    
    // this gets called for every event
    // usually the working horse ...
    
    // Clear memory
    memset( &_data, 0, sizeof(_data) );
    
    // try to get lcio collection (exits if collection is not available)
    // NOTE: if the AIDAProcessor is activated in your steering file and Marlin is linked with
    //      RAIDA you may get the message: "*** Break *** segmentation violation" followed by a
    //      stack-trace (generated by ROOT) in case the collection is unavailable. This happens
    //      because ROOT is somehow catching the exit signal commonly used to exit a program
    //      intentionally. Sorry if this messsage may confuse you. Please ignore it!
    LCCollection* colmcp = evt->getCollection( _colMCP ) ;
    LCCollection* colpfo = evt->getCollection( _colPFO ) ;
    LCCollection* colmcpforel = evt->getCollection( _colMCPFORelation ) ;
    
    
    // Alternativelly if you do not want Marlin to exit in case of a non-existing collection
    // use the following (commented out) code:
    //LCCollection* col = NULL;
    //try{
    //    col = evt->getCollection( _colName );
    //}
    //catch( lcio::DataNotAvailableException e )
    //{
    //    streamlog_out(WARNING) << _colName << " collection not available" << std::endl;
    //    col = NULL;
    //}
    
    // this will only be entered if the collection is available
    if( colmcp != NULL ){
        
        int nMCP = colmcp->getNumberOfElements()  ;
        _data.nmcps = nMCP;
        
        for(int i=0; i< nMCP ; i++){
            
            MCParticle* p = dynamic_cast<MCParticle*>( colmcp->getElementAt( i ) ) ;
            
            _data.mcp_e[i]     = p->getEnergy();
            float px = p->getMomentum()[0];
            float py = p->getMomentum()[1];
            float pz = p->getMomentum()[2];
            _data.mcp_px[i]    = px;
            _data.mcp_py[i]    = py;
            _data.mcp_pz[i]    = pz;
            TVector3 pv(px,py,pz);
            _data.mcp_phi[i]   = pv.Phi();
            //_data.mcp_theta[i] = pv.Theta(); // need to consider 2pi = 0.
            _data.mcp_theta[i] = TMath::ATan(pv.Perp()/pz);
            _data.mcp_chrg[i]  = p->getCharge();
            _data.mcp_pdg[i]   = p->getPDG();
            _data.mcp_genstatus[i] = p->getGeneratorStatus();
            _data.mcp_simstatus[i] = p->getSimulatorStatus();
            _data.mcp_iscreatedinsim[i] = p->isCreatedInSimulation();
            
        }
    }
    
    
    if( colpfo != NULL ){
        
        int nPFO = colpfo->getNumberOfElements()  ;
        _data.npfos = nPFO;
        
        int icalhits = 0;
        for(int i=0; i< nPFO ; i++){
            ReconstructedParticle* p = dynamic_cast<ReconstructedParticle*>( colpfo->getElementAt( i ) ) ;
            
            // TRK info
            const EVENT::TrackVec & trkvec = p->getTracks();
            // CAL info
            const EVENT::ClusterVec& clusvec = p->getClusters();
            // PID info
            const EVENT::ParticleIDVec& pidvec = p->getParticleIDs();
            
            // MC Relation
            MCParticle* mcr = 0;
            if( colmcpforel != NULL ){
                _navpfo = new LCRelationNavigator( colmcpforel );
                //int nmcr = _navpfo->getRelatedToWeights( p ).size();
                int nmcr = _navpfo->getRelatedToObjects( p ).size();
                _data.nmcr[i] = nmcr;
                if ( nmcr > 0 ) {
                    mcr = dynamic_cast< MCParticle *> ( _navpfo->getRelatedToObjects( p )[0] );
                    _data.mcr_weight[i] = _navpfo->getRelatedToWeights( p )[0];
                    _data.mcr_e[i]     = mcr->getEnergy();
                    float px = mcr->getMomentum()[0];
                    float py = mcr->getMomentum()[1];
                    float pz = mcr->getMomentum()[2];
                    _data.mcr_px[i]    = px;
                    _data.mcr_py[i]    = py;
                    _data.mcr_pz[i]    = pz;
                    TVector3 pv(px,py,pz);
                    _data.mcr_phi[i]   = pv.Phi();
                    //_data.mcr_theta[i] = pv.Theta(); // need to consider 2pi = 0.
                    _data.mcr_theta[i] = TMath::ATan(pv.Perp()/pz);
                    _data.mcr_chrg[i]  = mcr->getCharge();
                    _data.mcr_pdg[i]   = mcr->getPDG();
                    _data.mcr_genstatus[i] = mcr->getGeneratorStatus();
                    _data.mcr_simstatus[i] = mcr->getSimulatorStatus();
                    _data.mcr_iscreatedinsim[i] = mcr->isCreatedInSimulation();
                }
            }
            
            _data.pfo_e[i]     = p->getEnergy();
            float px = p->getMomentum()[0];
            float py = p->getMomentum()[1];
            float pz = p->getMomentum()[2];
            _data.pfo_px[i]    = px;
            _data.pfo_py[i]    = py;
            _data.pfo_pz[i]    = pz;
            TVector3 pv(px,py,pz);
            _data.pfo_phi[i]   = pv.Phi();
            //_data.pfo_theta[i] = pv.Theta(); // need to consider 2pi = 0.
            _data.pfo_theta[i] = TMath::ATan(pv.Perp()/pz);
            _data.pfo_chrg[i]  = p->getCharge();
            _data.pfo_pdg[i]   = p->getType();
            
            // fill first track info
            bool isTrkGamma = false;
            _data.pfo_ntrk[i]  = trkvec.size();
            if (trkvec.size()>0) {
                const Track* trk = trkvec[0];
                _data.pfo_d0[i] = trk->getD0();
                _data.pfo_z0[i] = trk->getZ0();
                _data.pfo_trkphi[i] = trk->getPhi();
                _data.pfo_omega[i] = trk->getOmega();
                _data.pfo_tanlambda[i] = trk->getTanLambda();
                _data.pfo_d0sig[i] = trk->getD0()/sqrt(trk->getCovMatrix()[0]);
                _data.pfo_z0sig[i] = trk->getZ0()/sqrt(trk->getCovMatrix()[9]);
            }
            if (trkvec.size() == 0) isTrkGamma = true; // gamma track hit
            
            // fill sum of cluster info
            int nclrs = clusvec.size();
            bool isCalGamma = false;
            _data.pfo_nclus[i] = nclrs;
            if (clusvec.size()>0) {
                float xsum = 0.;
                float ysum = 0.;
                float zsum = 0.;
                for ( ClusterVec::const_iterator iCluster=clusvec.begin();
                     iCluster!=clusvec.end(); ++iCluster) {
                    const Cluster* cls = *iCluster;
                    const float* xp = cls->getPosition();
                    xsum += xp[0];
                    ysum += xp[1];
                    zsum += xp[2];
                    _data.pfo_ecal_e[i]  += cls->getSubdetectorEnergies()[0];
                    _data.pfo_hcal_e[i]  += cls->getSubdetectorEnergies()[1];
                    _data.pfo_yoke_e[i]  += cls->getSubdetectorEnergies()[2];
                    _data.pfo_lcal_e[i]  += cls->getSubdetectorEnergies()[3];
                    _data.pfo_lhcal_e[i] += cls->getSubdetectorEnergies()[4];
                    _data.pfo_bcal_e[i]  += cls->getSubdetectorEnergies()[5];
                    
                    // Note that there is no hit info in DST files.
                    const EVENT::CalorimeterHitVec & calhits = cls->getCalorimeterHits();
                    for ( CalorimeterHitVec::const_iterator iCalhit=calhits.begin();
                         iCalhit!=calhits.end(); ++iCalhit) {
                        const CalorimeterHit* calhit = *iCalhit;
                        const float* xp = calhit->getPosition();
                        _data.clr_x[icalhits] = xp[0];
                        _data.clr_y[icalhits] = xp[1];
                        _data.clr_z[icalhits] = xp[2];
                        icalhits++;
                    }
                    
                } // end of cluster iteration
                
                _data.pfo_cal_x[i] = xsum / nclrs;
                _data.pfo_cal_y[i] = ysum / nclrs;
                _data.pfo_cal_z[i] = zsum / nclrs;
                
            }// end of cluster if
            if(_data.pfo_ecal_e[i]>8 && _data.pfo_hcal_e[i]==0) isCalGamma = true; // gamma cal hit
            
            
            // gamma selection
            bool isNotCharged = false;
            if(_data.pfo_chrg[i] == 0) isNotCharged = true;
            
            if(isNotCharged && isCalGamma){
                _data.pfo_gamma_e[i] = _data.pfo_e[i];
                
            }
            
            
            
            
            
            
            
            
            
            //pid info
            if (pidvec.size()>0){
                const ParticleID* partID = pidvec[0];
                _data.pfo_pid[i] =  partID->getPDG();
            }
            
            
        } // end of PFO loop
        
        _data.nclrhits = icalhits;
        
    } // end of PFO if
    
    //-- note: this will not be printed if compiled w/o MARLINDEBUG=1 !
    
    streamlog_out(DEBUG) << "   processing event: " << evt->getEventNumber()
    << "   in run:  " << evt->getRunNumber() << std::endl ;
    
    _data.evt = _nEvt;
    
    _evtdata->Fill();
    
    _nEvt ++ ;
}



void MPProcessor::check( LCEvent * evt ) { 
    // nothing to check here - could be used to fill checkplots in reconstruction processor
}


void MPProcessor::end(){ 
    
    _otfile->Write();
    
    //  std::cout << "MPProcessor::end()  " << name()
    //	    << " processed " << _nEvt << " events in " << _nRun << " runs "
    //	    << std::endl ;
    
}

void MPProcessor::makeNTuple() {
    
    // Output root file
    _otfile    = new TFile( _rootfilename.c_str() , "RECREATE" );
    
    EVTFILLDATA &d = _data;
    
    /** Define root tree
     */
    _evtdata  = new TTree( "evtdata" , "events" );
    _evtdata->Branch( "evt"             , &d.evt             , "evt/I"           );
    _evtdata->Branch( "npfos"           , &d.npfos           , "npfos/I"         );
    _evtdata->Branch( "pfo_e"           , &d.pfo_e           , "pfo_e[npfos]"          );
    _evtdata->Branch( "pfo_px"          , &d.pfo_px          , "pfo_px[npfos]"         );
    _evtdata->Branch( "pfo_py"          , &d.pfo_py          , "pfo_py[npfos]"         );
    _evtdata->Branch( "pfo_pz"          , &d.pfo_pz          , "pfo_pz[npfos]"         );
    _evtdata->Branch( "pfo_phi"         , &d.pfo_phi         , "pfo_phi[npfos]"        );
    _evtdata->Branch( "pfo_theta"       , &d.pfo_theta       , "pfo_theta[npfos]"      );
    _evtdata->Branch( "pfo_chrg"        , &d.pfo_chrg        , "pfo_chrg[npfos]"       );
    _evtdata->Branch( "pfo_pdg"         , &d.pfo_pdg         , "pfo_pdg[npfos]/I"      );
    _evtdata->Branch( "pfo_ntrk"        , &d.pfo_ntrk        , "pfo_ntrk[npfos]/I"     );
    _evtdata->Branch( "pfo_d0"          , &d.pfo_d0          , "pfo_d0[npfos]"         );
    _evtdata->Branch( "pfo_d0sig"       , &d.pfo_d0sig       , "pfo_d0sig[npfos]"      );
    _evtdata->Branch( "pfo_z0"          , &d.pfo_z0          , "pfo_z0[npfos]"         );
    _evtdata->Branch( "pfo_z0sig"       , &d.pfo_z0sig       , "pfo_z0sig[npfos]"      );
    _evtdata->Branch( "pfo_trkphi"      , &d.pfo_trkphi      , "pfo_trkphi[npfos]"     );
    _evtdata->Branch( "pfo_omega"       , &d.pfo_omega       , "pfo_omega[npfos]"      );
    _evtdata->Branch( "pfo_tanlambda"   , &d.pfo_tanlambda   , "pfo_tanlambda[npfos]"  );
    _evtdata->Branch( "pfo_nclus"       , &d.pfo_nclus       , "pfo_nclus[npfos]/I"    );
    _evtdata->Branch( "pfo_cal_x"       , &d.pfo_cal_x       , "pfo_cal_x[npfos]"      );
    _evtdata->Branch( "pfo_cal_y"       , &d.pfo_cal_y       , "pfo_cal_y[npfos]"      );
    _evtdata->Branch( "pfo_cal_z"       , &d.pfo_cal_z       , "pfo_cal_z[npfos]"      );
    _evtdata->Branch( "pfo_ecal_e"      , &d.pfo_ecal_e      , "pfo_ecal_e[npfos]"     );
    _evtdata->Branch( "pfo_hcal_e"      , &d.pfo_hcal_e      , "pfo_hcal_e[npfos]"     );
    _evtdata->Branch( "pfo_yoke_e"      , &d.pfo_yoke_e      , "pfo_yoke_e[npfos]"     );
    _evtdata->Branch( "pfo_lcal_e"      , &d.pfo_lcal_e      , "pfo_lcal_e[npfos]"     );
    _evtdata->Branch( "pfo_lhcal_e"     , &d.pfo_lhcal_e     , "pfo_lhcal_e[npfos]"    );
    _evtdata->Branch( "pfo_bcal_e"      , &d.pfo_bcal_e      , "pfo_bcal_e[npfos]"     );
    
    _evtdata->Branch( "nmcr"            , &d.nmcr            , "nmcr[npfos]/I"         );
    _evtdata->Branch( "mcr_weight"      , &d.mcr_weight      , "mcr_weight[npfos]"     );
    _evtdata->Branch( "mcr_e"           , &d.mcr_e           , "mcr_e[npfos]"          );
    _evtdata->Branch( "mcr_px"          , &d.mcr_px          , "mcr_px[npfos]"         );
    _evtdata->Branch( "mcr_py"          , &d.mcr_py          , "mcr_py[npfos]"         );
    _evtdata->Branch( "mcr_pz"          , &d.mcr_pz          , "mcr_pz[npfos]"         );
    _evtdata->Branch( "mcr_phi"         , &d.mcr_phi         , "mcr_phi[npfos]"        );
    _evtdata->Branch( "mcr_theta"       , &d.mcr_theta       , "mcr_theta[npfos]"      );
    _evtdata->Branch( "mcr_chrg"        , &d.mcr_chrg        , "mcr_chrg[npfos]"       );
    _evtdata->Branch( "mcr_pdg"         , &d.mcr_pdg         , "mcr_pdg[npfos]/I"      );
    _evtdata->Branch( "mcr_genstatus"   , &d.mcr_genstatus   , "mcr_genstatus[npfos]/I");
    _evtdata->Branch( "mcr_simstatus"   , &d.mcr_simstatus   , "mcr_simstatus[npfos]/I");
    _evtdata->Branch( "mcr_iscreatedinsim"   , &d.mcr_iscreatedinsim   , "mcr_iscreatedinsim[npfos]/O");
    
    _evtdata->Branch( "nmcps"           , &d.nmcps           , "nmcps/I"               );
    _evtdata->Branch( "mcp_e"           , &d.mcp_e           , "mcp_e[nmcps]"          );
    _evtdata->Branch( "mcp_px"          , &d.mcp_px          , "mcp_px[nmcps]"         );
    _evtdata->Branch( "mcp_py"          , &d.mcp_py          , "mcp_py[nmcps]"         );
    _evtdata->Branch( "mcp_pz"          , &d.mcp_pz          , "mcp_pz[nmcps]"         );
    _evtdata->Branch( "mcp_phi"         , &d.mcp_phi         , "mcp_phi[nmcps]"        );
    _evtdata->Branch( "mcp_theta"       , &d.mcp_theta       , "mcp_theta[nmcps]"      );
    _evtdata->Branch( "mcp_chrg"        , &d.mcp_chrg        , "mcp_chrg[nmcps]"       );
    _evtdata->Branch( "mcp_pdg"         , &d.mcp_pdg         , "mcp_pdg[nmcps]/I"      );
    _evtdata->Branch( "mcp_genstatus"   , &d.mcp_genstatus   , "mcp_genstatus[nmcps]/I");
    _evtdata->Branch( "mcp_simstatus"   , &d.mcp_simstatus   , "mcp_simstatus[nmcps]/I");
    _evtdata->Branch( "mcp_iscreatedinsim"   , &d.mcp_iscreatedinsim   , "mcp_iscreatedinsim[nmcps]/O");
    _evtdata->Branch( "nclrhits"        , &d.nclrhits        , "nclrhits/I"            );
    _evtdata->Branch( "clr_x"           , &d.clr_x           , "clr_x[nclrhits]"       );
    _evtdata->Branch( "clr_y"           , &d.clr_y           , "clr_y[nclrhits]"       );
    _evtdata->Branch( "clr_z"           , &d.clr_z           , "clr_z[nclrhits]"       );
    
    _evtdata->Branch( "pfo_pid"           , &d.pfo_pid       , "pfo_pid[npfos]"       );
    return;
    
}
