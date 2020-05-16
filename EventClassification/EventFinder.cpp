/**
 *  @copyright Copyright 2020 The J-PET Framework Authors. All rights reserved.
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may find a copy of the License in the LICENCE file.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  @file EventFinder.cpp
 */

using namespace std;

#include <JPetOptionsTools/JPetOptionsTools.h>
#include <JPetWriter/JPetWriter.h>
#include "EventCategorizerTools.h"
#include "EventFinder.h"
#include <iostream>

using namespace jpet_options_tools;

EventFinder::EventFinder(const char* name): JPetUserTask(name) {}

EventFinder::~EventFinder() {}

bool EventFinder::init()
{
  INFO("Event finding started.");

  fOutputEvents = new JPetTimeWindow("JPetEvent");

  // Reading values from the user options if available
  // Getting bool for using corrupted hits
  if (isOptionSet(fParams.getOptions(), kUseCorruptedHitsParamKey)) {
    fUseCorruptedHits = getOptionAsBool(fParams.getOptions(), kUseCorruptedHitsParamKey);
    if(fUseCorruptedHits){
      WARNING("Event Finder is using Corrupted Hits, as set by the user");
    } else{
      WARNING("Event Finder is NOT using Corrupted Hits, as set by the user");
    }
  } else {
    WARNING("Event Finder is not using Corrupted Hits (default option)");
  }

  // Event time window
  if (isOptionSet(fParams.getOptions(), kEventTimeParamKey)) {
    fEventTimeWindow = getOptionAsFloat(fParams.getOptions(), kEventTimeParamKey);
  } else {
    WARNING(Form(
      "No value of the %s parameter provided by the user. Using default value of %lf.",
      kEventTimeParamKey.c_str(), fEventTimeWindow
    ));
  }

  if (isOptionSet(fParams.getOptions(), kABTimeDiffParamKey)) {
    fABTimeDiff = getOptionAsFloat(fParams.getOptions(), kABTimeDiffParamKey);
  } else {
    WARNING(Form(
      "No value of the %s parameter provided by the user. Using default value of %lf.",
      kABTimeDiffParamKey.c_str(), fABTimeDiff
    ));
  }

  if (isOptionSet(fParams.getOptions(), kTOTCutMinParamKey)) {
    fTOTCutMin = getOptionAsFloat(fParams.getOptions(), kTOTCutMinParamKey);
  } else {
    WARNING(Form(
      "No value of the %s parameter provided by the user. Using default value of %lf.",
      kTOTCutMinParamKey.c_str(), fTOTCutMin
    ));
  }

  if (isOptionSet(fParams.getOptions(), kTOTCutMaxParamKey)) {
    fTOTCutMax = getOptionAsFloat(fParams.getOptions(), kTOTCutMaxParamKey);
  } else {
    WARNING(Form(
      "No value of the %s parameter provided by the user. Using default value of %lf.",
      kTOTCutMaxParamKey.c_str(), fTOTCutMax
    ));
  }

  // Minimum number of hits in an event to save an event
  if (isOptionSet(fParams.getOptions(), kEventMinMultiplicity)) {
    fMinMultiplicity = getOptionAsInt(fParams.getOptions(), kEventMinMultiplicity);
  } else {
    WARNING(Form(
      "No value of the %s parameter provided by the user. Using default value of %d.",
      kEventMinMultiplicity.c_str(), fMinMultiplicity
    ));
  }

  // Getting bool for saving histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey)){
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }

  // Initialize histograms
  if (fSaveControlHistos) { initialiseHistograms(); }
  return true;
}

bool EventFinder::exec()
{
  vector<JPetEvent> eventVec;
  if (auto timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent)) {
    saveEvents(buildEvents(*timeWindow));
  } else { return false; }
  return true;
}

bool EventFinder::terminate()
{
  INFO("Event fiding ended.");
  return true;
}

void EventFinder::saveEvents(const vector<JPetEvent>& events)
{
  for (const auto& event : events){
    fOutputEvents->add<JPetEvent>(event);
  }
}

/**
 * Main method of building Events - Hit in the Time slot are groupped
 * within time parameter, that can be set by the user
 */
vector<JPetEvent> EventFinder::buildEvents(const JPetTimeWindow& timeWindow)
{
  vector<JPetEvent> eventVec;
  const unsigned int nHits = timeWindow.getNumberOfEvents();
  unsigned int count = 0;
  while(count<nHits){
    auto hit = dynamic_cast<const JPetHit&>(timeWindow.operator[](count));
    if(!fUseCorruptedHits && hit.getRecoFlag()==JPetHit::Corrupted){
      count++;
      continue;
    }
    // Creating new event with the first hit
    JPetEvent event;
    event.setEventType(JPetEventType::kUnknown);
    event.addHit(hit);
    if(hit.getRecoFlag() == JPetHit::Good) {
      event.setRecoFlag(JPetEvent::Good);
    } else if(hit.getRecoFlag() == JPetHit::Corrupted){
      event.setRecoFlag(JPetEvent::Corrupted);
    }
    // Checking, if following hits fulfill time window condition,
    // then moving interator
    unsigned int nextCount = 1;
    while(count+nextCount < nHits){
      auto nextHit = dynamic_cast<const JPetHit&>(timeWindow.operator[](count+nextCount));
      if (fabs(nextHit.getTime() - hit.getTime()) < fEventTimeWindow) {
        if(nextHit.getRecoFlag() == JPetHit::Corrupted) {
          event.setRecoFlag(JPetEvent::Corrupted);
        }
        event.addHit(nextHit);
        nextCount++;

        if(fSaveControlHistos){
          if(hit.getScin().getID() != nextHit.getScin().getID()){
            // Coincidental hits
            auto multiHit =
            hit.getSignalA().getRawSignals().size()
            + hit.getSignalB().getRawSignals().size();
            auto multiNextHit =
            nextHit.getSignalA().getRawSignals().size()
            + nextHit.getSignalB().getRawSignals().size();

            // all hits
            getStatistics().getHisto1D("coin_hit_sig_multi")->Fill(multiHit);
            getStatistics().getHisto1D("coin_hit_sig_multi")->Fill(multiNextHit);

            getStatistics().getHisto1D("coin_hits_tdiff")
            ->Fill(hit.getTimeDiff());
            getStatistics().getHisto1D("coin_hits_tdiff")
            ->Fill(nextHit.getTimeDiff());

            getStatistics().getHisto1D("coin_hits_tot")
            ->Fill(hit.getEnergy()/((float) multiHit));
            getStatistics().getHisto1D("coin_hits_tot")
            ->Fill(nextHit.getEnergy()/((float) multiNextHit));

            // Per scin ID
            getStatistics().getHisto1D(Form(
              "coin_tdiff_scin_%d", hit.getScin().getID()
            ))->Fill(hit.getTimeDiff());
            getStatistics().getHisto1D(Form(
              "coin_tdiff_scin_%d", nextHit.getScin().getID()
            ))->Fill(nextHit.getTimeDiff());

            getStatistics().getHisto1D(Form(
              "coin_tot_scin_%d", hit.getScin().getID()
            ))->Fill(hit.getEnergy()/((float) multiHit));

            getStatistics().getHisto1D(Form(
              "coin_tot_scin_%d", nextHit.getScin().getID()
            ))->Fill(nextHit.getEnergy()/((float) multiNextHit));

            getStatistics().getHisto2D(Form("tdiff_tot_scin_%d", hit.getScin().getID()))
            ->Fill(hit.getTimeDiff(), hit.getEnergy()/((float) multiHit));

            getStatistics().getHisto2D(Form("tdiff_tot_scin_%d", nextHit.getScin().getID()))
            ->Fill(nextHit.getTimeDiff(), nextHit.getEnergy()/((float) multiNextHit));

            // Per scin ID and multiplicity
            getStatistics().getHisto1D(Form(
              "coin_tdiff_scin_%d_m_%d",
              hit.getScin().getID(), ((int) multiHit)
            ))->Fill(hit.getTimeDiff());
            getStatistics().getHisto1D(Form(
              "coin_tdiff_scin_%d_m_%d",
              nextHit.getScin().getID(), ((int) multiNextHit)
            ))->Fill(nextHit.getTimeDiff());

            getStatistics().getHisto1D(Form(
              "coin_tot_scin_%d_m_%d",
              hit.getScin().getID(), ((int) multiHit)
            ))->Fill(hit.getEnergy()/((float) multiHit));
            getStatistics().getHisto1D(Form(
              "coin_tot_scin_%d_m_%d",
              nextHit.getScin().getID(), ((int) multiNextHit)
            ))->Fill(nextHit.getEnergy()/((float) multiNextHit));

            getStatistics().getHisto2D(Form(
              "tdiff_tot_scin_%d_m_%d", hit.getScin().getID(), ((int) multiHit)
            ))->Fill(hit.getTimeDiff(), hit.getEnergy()/((float) multiHit));

            getStatistics().getHisto2D(Form(
              "tdiff_tot_scin_%d_m_%d", nextHit.getScin().getID(), ((int) multiNextHit)
            ))->Fill(nextHit.getTimeDiff(), nextHit.getEnergy()/((float) multiNextHit));
          }
        }
      } else {
        getStatistics().getHisto1D("hits_rejected_tdiff")
        ->Fill(fabs(nextHit.getTime() - hit.getTime()));
        break;
      }
    }
    count+=nextCount;
    if(fSaveControlHistos) {
      getStatistics().getHisto1D("hits_per_event_all")->Fill(event.getHits().size());
      if(event.getRecoFlag()==JPetEvent::Good){
        getStatistics().getHisto1D("good_vs_bad_events")->Fill(1);
      } else if(event.getRecoFlag()==JPetEvent::Corrupted){
        getStatistics().getHisto1D("good_vs_bad_events")->Fill(2);
      } else {
        getStatistics().getHisto1D("good_vs_bad_events")->Fill(3);
      }
    }
    if(event.getHits().size() >= fMinMultiplicity){
      eventVec.push_back(event);
      if(fSaveControlHistos) {
        getStatistics().getHisto1D("hits_per_event_selected")->Fill(event.getHits().size());
      }
    }
  }
  return eventVec;
}

void EventFinder::initialiseHistograms(){

  getStatistics().createHistogram(
    new TH1F("hits_rejected_tdiff", "Rej tdiff",
    200, 0.0, 500000.0)
  );
  getStatistics().getHisto1D("hits_rejected_tdiff")->GetXaxis()->SetTitle("Time difference [ps]");
  getStatistics().getHisto1D("hits_rejected_tdiff")->GetYaxis()->SetTitle("Number of Hit Pairs");

  getStatistics().createHistogram(
    new TH1F("hits_per_event_all", "Number of Hits in an all Events", 20, 0.5, 20.5)
  );
  getStatistics().getHisto1D("hits_per_event_all")->GetXaxis()->SetTitle("Hits in Event");
  getStatistics().getHisto1D("hits_per_event_all")->GetYaxis()->SetTitle("Number of Hits");

  getStatistics().createHistogram(
    new TH1F("hits_per_event_selected", "Number of Hits in selected Events (min. multiplicity)",
    20, fMinMultiplicity-0.5, fMinMultiplicity+19.5)
  );
  getStatistics().getHisto1D("hits_per_event_selected")->GetXaxis()->SetTitle("Hits in Event");
  getStatistics().getHisto1D("hits_per_event_selected")->GetYaxis()->SetTitle("Number of Hits");

  //////////////////////////////////////////////////////////////////////////////
  getStatistics().createHistogram(new TH1F(
    "coin_hit_sig_multi", "Number of signals from SiPMs in pairs of coincidental hits",
    10, -0.5, 9.5
  ));
  getStatistics().getHisto1D("coin_hit_sig_multi")
  ->GetXaxis()->SetTitle("Number of SiPM signals");
  getStatistics().getHisto1D("coin_hit_sig_multi")
  ->GetYaxis()->SetTitle("Number of Hits");

  getStatistics().createHistogram(new TH1F(
    "coin_hits_tdiff", "Coincidental hits time difference",
    200, -1.1 * fABTimeDiff, 1.1 * fABTimeDiff
  ));
  getStatistics().getHisto1D("coin_hits_tdiff")
  ->GetXaxis()->SetTitle("Time difference [ps]");
  getStatistics().getHisto1D("coin_hits_tdiff")
  ->GetYaxis()->SetTitle("Number of Hit Pairs");

  getStatistics().createHistogram(new TH1F(
    "coin_hits_tot", "Coincidental hits TOT",
    200, 0.0, 375000.0
  ));
  getStatistics().getHisto1D("coin_hits_tot")
  ->GetXaxis()->SetTitle("Time over Threshold [ps]");
  getStatistics().getHisto1D("coin_hits_tot")
  ->GetYaxis()->SetTitle("Number of Hit Pairs");

  auto minScinID = getParamBank().getScins().begin()->first;
  auto maxScinID = 215;

  // Time diff and TOT per scin per multi
  for(int scinID = minScinID; scinID <= maxScinID; scinID++){
    if(scinID != 201 && scinID != 213) { continue; }

    getStatistics().createHistogram(new TH1F(
      Form("coin_tdiff_scin_%d", scinID),
      Form("Hits coincidence, time difference, scin %d", scinID),
      200, -1.1 * fABTimeDiff, 1.1 * fABTimeDiff
    ));
    getStatistics().getHisto1D(Form("coin_tdiff_scin_%d", scinID))
    ->GetXaxis()->SetTitle("Time difference [ps]");
    getStatistics().getHisto1D(Form("coin_tdiff_scin_%d", scinID))
    ->GetYaxis()->SetTitle("Number of Hits");

    getStatistics().createHistogram(new TH1F(
      Form("coin_tot_scin_%d", scinID),
      Form("Hits coincidence, TOT divided by multiplicity, scin %d", scinID),
      200, 0.0, 375000.0
    ));
    getStatistics().getHisto1D(Form("coin_tot_scin_%d", scinID))
    ->GetXaxis()->SetTitle("Time over Threshold [ps]");
    getStatistics().getHisto1D(Form("coin_tot_scin_%d", scinID))
    ->GetYaxis()->SetTitle("Number of Hits");

    getStatistics().createHistogram(new TH2F(
      Form("tdiff_tot_scin_%d", scinID),
      Form("Hits coincidence, time difference vs. TOT divided by multiplicity, scin %d",  scinID),
      200, -1.1 * fABTimeDiff, 1.1 * fABTimeDiff, 200, 0.0, 375000.0
    ));
    getStatistics().getHisto2D(Form("tdiff_tot_scin_%d", scinID))
    ->GetXaxis()->SetTitle("Time difference [ps]");
    getStatistics().getHisto2D(Form("tdiff_tot_scin_%d", scinID))
    ->GetYaxis()->SetTitle("Time over Threshold [ps]");

    for(int multi = 2; multi <=8; multi++){

      getStatistics().createHistogram(new TH1F(
        Form("coin_tdiff_scin_%d_m_%d", scinID, multi),
        Form("Hits coincidence, time difference, scin %d, multi %d", scinID, multi),
        200, -1.1 * fABTimeDiff, 1.1 * fABTimeDiff
      ));
      getStatistics().getHisto1D(Form("coin_tdiff_scin_%d_m_%d", scinID, multi))
      ->GetXaxis()->SetTitle("Time difference [ps]");
      getStatistics().getHisto1D(Form("coin_tdiff_scin_%d_m_%d", scinID, multi))
      ->GetYaxis()->SetTitle("Number of Hits");

      getStatistics().createHistogram(new TH1F(
        Form("coin_tot_scin_%d_m_%d", scinID, multi),
        Form("Hits coincidence, TOT divided by multiplicity, scin %d multi %d", scinID, multi),
        200, 0.0, 375000.0
      ));
      getStatistics().getHisto1D(Form("coin_tot_scin_%d_m_%d", scinID, multi))
      ->GetXaxis()->SetTitle("Time over Threshold [ps]");
      getStatistics().getHisto1D(Form("coin_tot_scin_%d_m_%d", scinID, multi))
      ->GetYaxis()->SetTitle("Number of Hits");

      getStatistics().createHistogram(new TH2F(
        Form("tdiff_tot_scin_%d_m_%d", scinID, multi),
        Form("Hits coincidence, time difference vs. TOT divided by multiplicity, scin %d multi %d",
        scinID, multi),
        200, -1.1 * fABTimeDiff, 1.1 * fABTimeDiff, 200, 0.0, 375000.0
      ));
      getStatistics().getHisto2D(Form("tdiff_tot_scin_%d_m_%d", scinID, multi))
      ->GetXaxis()->SetTitle("Time difference [ps]");
      getStatistics().getHisto2D(Form("tdiff_tot_scin_%d_m_%d", scinID, multi))
      ->GetYaxis()->SetTitle("Time over Threshold [ps]");

    }
  }

  getStatistics().createHistogram(new TH1F(
    "good_vs_bad_events", "Number of good and corrupted Events created", 3, 0.5, 3.5
  ));
  getStatistics().getHisto1D("good_vs_bad_events")->GetXaxis()->SetBinLabel(1, "GOOD");
  getStatistics().getHisto1D("good_vs_bad_events")->GetXaxis()->SetBinLabel(2, "CORRUPTED");
  getStatistics().getHisto1D("good_vs_bad_events")->GetXaxis()->SetBinLabel(3, "UNKNOWN");
  getStatistics().getHisto1D("good_vs_bad_events")->GetYaxis()->SetTitle("Number of Events");
}
