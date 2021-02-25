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
 *  @file HitFinder.cpp
 */

#include <JPetAnalysisTools/JPetAnalysisTools.h>
#include <JPetOptionsTools/JPetOptionsTools.h>
#include <JPetWriter/JPetWriter.h>

#include <boost/property_tree/json_parser.hpp>

#include "CalibrationTools.h"
#include "HitFinder.h"
#include "HitFinderTools.h"
#include <map>
#include <string>
#include <vector>

using namespace jpet_options_tools;

HitFinder::HitFinder(const char* name) : JPetUserTask(name) {}

HitFinder::~HitFinder() {}

bool HitFinder::init()
{
  INFO("Hit finding Started");
  fOutputEvents = new JPetTimeWindow("JPetHit");

  // Reading values from the user options if available
  // Getting bools for saving control and calibration histograms
  if (isOptionSet(fParams.getOptions(), kSaveControlHistosParamKey))
  {
    fSaveControlHistos = getOptionAsBool(fParams.getOptions(), kSaveControlHistosParamKey);
  }
  if (isOptionSet(fParams.getOptions(), kSaveCalibHistosParamKey))
  {
    fSaveCalibHistos = getOptionAsBool(fParams.getOptions(), kSaveCalibHistosParamKey);
  }

  // Reading file with effective light velocit and TOF synchronization constants to property tree
  if (isOptionSet(fParams.getOptions(), kConstantsFileParamKey))
  {
    boost::property_tree::read_json(getOptionAsString(fParams.getOptions(), kConstantsFileParamKey), fConstansTree);
  }

  // Allowed time difference between signals on A and B sides
  if (isOptionSet(fParams.getOptions(), kABTimeDiffParamKey))
  {
    fABTimeDiff = getOptionAsDouble(fParams.getOptions(), kABTimeDiffParamKey);
  }
  else
  {
    WARNING(Form("No value of the %s parameter provided by the user. Using default value of %lf.", kABTimeDiffParamKey.c_str(), fABTimeDiff));
  }

  // Control histograms
  if (fSaveControlHistos)
  {
    initialiseHistograms();
  }

  return true;
}

bool HitFinder::exec()
{
  if (auto timeWindow = dynamic_cast<const JPetTimeWindow* const>(fEvent))
  {
    auto signalsBySlot = HitFinderTools::getSignalsByScin(timeWindow);
    auto allHits = HitFinderTools::matchAllSignals(signalsBySlot, fABTimeDiff, fConstansTree, getStatistics(), fSaveControlHistos);
    if (allHits.size() > 0)
    {
      saveHits(allHits);
    }
  }
  else
    return false;
  return true;
}

bool HitFinder::terminate()
{
  INFO("Hit finding ended");
  return true;
}

void HitFinder::saveHits(const std::vector<JPetHit>& hits)
{
  auto sortedHits = JPetAnalysisTools::getHitsOrderedByTime(hits);

  if (fSaveControlHistos)
  {
    getStatistics().getHisto1D("hits_tslot")->Fill(hits.size());
  }

  for (auto& hit : sortedHits)
  {
    fOutputEvents->add<JPetHit>(hit);

    if (fSaveControlHistos)
    {
      int multi = hit.getSignalA().getRawSignals().size() + hit.getSignalB().getRawSignals().size();
      int scinID = hit.getScin().getID();

      getStatistics().getHisto1D("hits_scin")->Fill(scinID);

      getStatistics().getHisto2D("hit_pos_XY")->Fill(hit.getPosY(), hit.getPosX());
      getStatistics().getHisto2D("hit_pos_z")->Fill(scinID, hit.getPosZ());

      getStatistics().getHisto1D("hit_multi")->Fill(multi);
      getStatistics().getHisto2D("hit_multi_scin")->Fill(scinID, multi);

      getStatistics().getHisto2D("hit_tdiff_scin")->Fill(scinID, hit.getTimeDiff());
      getStatistics().getHisto2D(Form("hit_tdiff_scin_m_%d", multi))->Fill(scinID, hit.getTimeDiff());

      if (hit.getEnergy() != 0.0)
      {
        getStatistics().getHisto2D("hit_tot_scin")->Fill(scinID, hit.getQualityOfEnergy());
        getStatistics().getHisto2D("hit_tot_scin_norm")->Fill(scinID, hit.getEnergy());
        getStatistics().getHisto2D(Form("hit_tot_scin_m_%d", multi))->Fill(scinID, hit.getQualityOfEnergy());
        getStatistics().getHisto2D(Form("hit_tot_scin_m_%d_norm", multi))->Fill(scinID, hit.getEnergy());
      }

      // Checking tDiff and TOT per SiPM mtx position
      for (int mtxPos = 1; mtxPos <= 4; mtxPos++)
      {
        auto signalsMapA = hit.getSignalA().getRawSignals();
        auto signalsMapB = hit.getSignalB().getRawSignals();
        auto searchA = signalsMapA.find(mtxPos);
        auto searchB = signalsMapB.find(mtxPos);

        if (searchA != signalsMapA.end() && searchB != signalsMapB.end())
        {
          auto leadsA = searchA->second.getPoints(JPetSigCh::Leading, JPetRawSignal::ByThrNum);
          auto trailsA = searchA->second.getPoints(JPetSigCh::Trailing, JPetRawSignal::ByThrNum);
          auto leadsB = searchB->second.getPoints(JPetSigCh::Leading, JPetRawSignal::ByThrNum);
          auto trailsB = searchB->second.getPoints(JPetSigCh::Trailing, JPetRawSignal::ByThrNum);

          double totSum = 0.0;
          //
          //   for (int thr = 0; thr < leadsA.size() && thr < leadsB.size(); ++thr)
          //   {
          //     double tDiff = leadsB.at(thr).getTime() - leadsA.at(thr).getTime();
          //     double totA = trailsA.at(thr).getTime() - leadsA.at(thr).getTime();
          //     double totB = trailsB.at(thr).getTime() - leadsB.at(thr).getTime();
          //     totSum += totA + totB;
          //     getStatistics().getHisto2D(Form("hit_tdiff_thr%d_scin_mtx_pos_%d", thr + 1, mtxPos))->Fill(scinID, tDiff);
          //     getStatistics().getHisto2D(Form("hit_tot_thr%d_scin_mtx_pos_%d", thr + 1, mtxPos))->Fill(scinID, totA + totB);
          //   }
          //
          //   getStatistics().getHisto2D(Form("hit_tot_sum_scin_mtx_pos_%d", mtxPos))->Fill(scinID, totSum);
        }
      }
    }
  }
}

void HitFinder::initialiseHistograms()
{

  auto minScinID = getParamBank().getScins().begin()->first;
  auto maxScinID = getParamBank().getScins().rbegin()->first;

  getStatistics().createHistogram(new TH1F("hits_tslot", "Number of Hits in Time Window", 60, 0.5, 60.5));
  getStatistics().getHisto1D("hits_tslot")->GetXaxis()->SetTitle("Hits in Time Slot");
  getStatistics().getHisto1D("hits_tslot")->GetYaxis()->SetTitle("Number of Time Slots");

  getStatistics().createHistogram(
      new TH1F("hits_scin", "Number of Hits in Scintillators", maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5));
  getStatistics().getHisto1D("hits_scin")->GetXaxis()->SetTitle("Scin ID");
  getStatistics().getHisto1D("hits_scin")->GetYaxis()->SetTitle("Number of Hits");

  getStatistics().createHistogram(new TH2F("hit_pos_XY", "Hit Position XY projection", 101, -50.5, 50.5, 101, -50.5, 50.5));
  getStatistics().getHisto2D("hit_pos_XY")->GetXaxis()->SetTitle("Y [cm]");
  getStatistics().getHisto2D("hit_pos_XY")->GetYaxis()->SetTitle("X [cm]");

  getStatistics().createHistogram(
      new TH2F("hit_pos_z", "Hit Position per Scintillator ID", maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 201, -50.0, 50.0));
  getStatistics().getHisto2D("hit_pos_z")->GetXaxis()->SetTitle("Scintillator ID");
  getStatistics().getHisto2D("hit_pos_z")->GetYaxis()->SetTitle("Hit z-axis position [cm]");

  getStatistics().createHistogram(new TH1F("hit_multi", "Number of signals from SiPMs in created hit", 12, -0.5, 11.5));
  getStatistics().getHisto1D("hit_multi")->GetXaxis()->SetTitle("Number of signals");
  getStatistics().getHisto1D("hit_multi")->GetYaxis()->SetTitle("Number of Hits");

  getStatistics().createHistogram(new TH2F("hit_multi_scin", "Number of signals from SiPMs in created Hit per Scin", maxScinID - minScinID + 1,
                                           minScinID - 0.5, maxScinID + 0.5, 12, -0.5, 11.5));
  getStatistics().getHisto2D("hit_multi_scin")->GetXaxis()->SetTitle("Scintillator ID");
  getStatistics().getHisto2D("hit_multi_scin")->GetYaxis()->SetTitle("Signal multiplicity [ps]");

  // Time diff and TOT per scin
  getStatistics().createHistogram(new TH2F("hit_tdiff_scin", "Hit Time Difference per Scintillator ID", maxScinID - minScinID + 1, minScinID - 0.5,
                                           maxScinID + 0.5, 201, -1.1 * fABTimeDiff, 1.1 * fABTimeDiff));
  getStatistics().getHisto2D("hit_tdiff_scin")->GetXaxis()->SetTitle("Scintillator ID");
  getStatistics().getHisto2D("hit_tdiff_scin")->GetYaxis()->SetTitle("A-B time difference [ps]");

  getStatistics().createHistogram(new TH2F("hit_tot_scin", "Hit TOT divided by multiplicity, all hits", maxScinID - minScinID + 1, minScinID - 0.5,
                                           maxScinID + 0.5, 200, 0.0, 400000.0));
  getStatistics().getHisto2D("hit_tot_scin")->GetXaxis()->SetTitle("Scintillator ID");
  getStatistics().getHisto2D("hit_tot_scin")->GetYaxis()->SetTitle("Time over Threshold [ps]");

  getStatistics().createHistogram(new TH2F("hit_tot_scin_norm", "Normalized Hit TOT divided by multiplicity, all hits", maxScinID - minScinID + 1,
                                           minScinID - 0.5, maxScinID + 0.5, 200, 0.0, 400000.0));
  getStatistics().getHisto2D("hit_tot_scin_norm")->GetXaxis()->SetTitle("Scintillator ID");
  getStatistics().getHisto2D("hit_tot_scin_norm")->GetYaxis()->SetTitle("Normalized Time over Threshold [ps]");

  // Time diff and TOT per multiplicity
  for (int multi = 2; multi <= 8; multi++)
  {
    getStatistics().createHistogram(new TH2F(Form("hit_tdiff_scin_m_%d", multi), Form("Hit time difference per scin, multiplicity %d", multi),
                                             maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 300, -1.1 * fABTimeDiff,
                                             1.1 * fABTimeDiff));
    getStatistics().getHisto2D(Form("hit_tdiff_scin_m_%d", multi))->GetXaxis()->SetTitle("Scintillator ID");
    getStatistics().getHisto2D(Form("hit_tdiff_scin_m_%d", multi))->GetYaxis()->SetTitle("A-B time difference [ps]");

    getStatistics().createHistogram(new TH2F(Form("hit_tot_scin_m_%d", multi), Form("Hit TOT divided by multiplicity, multiplicity %d", multi),
                                             maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 200, 0.0, 400000.0));
    getStatistics().getHisto2D(Form("hit_tot_scin_m_%d", multi))->GetXaxis()->SetTitle("Scintillator ID");
    getStatistics().getHisto2D(Form("hit_tot_scin_m_%d", multi))->GetYaxis()->SetTitle("Time over Threshold [ps]");

    getStatistics().createHistogram(new TH2F(Form("hit_tot_scin_m_%d_norm", multi),
                                             Form("Normalized Hit TOT divided by multiplicity, multiplicity %d", multi), maxScinID - minScinID + 1,
                                             minScinID - 0.5, maxScinID + 0.5, 200, 0.0, 400000.0));
    getStatistics().getHisto2D(Form("hit_tot_scin_m_%d", multi))->GetXaxis()->SetTitle("Scintillator ID");
    getStatistics().getHisto2D(Form("hit_tot_scin_m_%d", multi))->GetYaxis()->SetTitle("Time over Threshold [ps]");
  }

  // Time diff and TOT per SiPM position in matrix
  for (int mtxPos = 1; mtxPos <= 4; mtxPos++)
  {
    getStatistics().createHistogram(new TH2F(Form("hit_tdiff_thr1_scin_mtx_pos_%d", mtxPos),
                                             Form("Hit time difference of times at THR2 per scin for SiPMs with matrix position %d", mtxPos),
                                             maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 300, -1.1 * fABTimeDiff,
                                             1.1 * fABTimeDiff));
    getStatistics().getHisto2D(Form("hit_tdiff_thr1_scin_mtx_pos_%d", mtxPos))->GetXaxis()->SetTitle("Scintillator ID");
    getStatistics().getHisto2D(Form("hit_tdiff_thr1_scin_mtx_pos_%d", mtxPos))->GetYaxis()->SetTitle("A-B time difference [ps]");

    getStatistics().createHistogram(new TH2F(Form("hit_tdiff_thr2_scin_mtx_pos_%d", mtxPos),
                                             Form("Hit time difference of times at THR2 per scin for SiPMs with matrix position %d", mtxPos),
                                             maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 300, -1.1 * fABTimeDiff,
                                             1.1 * fABTimeDiff));
    getStatistics().getHisto2D(Form("hit_tdiff_thr2_scin_mtx_pos_%d", mtxPos))->GetXaxis()->SetTitle("Scintillator ID");
    getStatistics().getHisto2D(Form("hit_tdiff_thr2_scin_mtx_pos_%d", mtxPos))->GetYaxis()->SetTitle("A-B time difference [ps]");

    getStatistics().createHistogram(new TH2F(Form("hit_tot_thr1_scin_mtx_pos_%d", mtxPos),
                                             Form("Hit TOT for times at THR1 of SiPM signals on matrix position %d", mtxPos),
                                             maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 200, 0.0, 400000.0));
    getStatistics().getHisto2D(Form("hit_tot_thr1_scin_mtx_pos_%d", mtxPos))->GetXaxis()->SetTitle("Scintillator ID");
    getStatistics().getHisto2D(Form("hit_tot_thr1_scin_mtx_pos_%d", mtxPos))->GetYaxis()->SetTitle("Time over Threshold [ps]");

    getStatistics().createHistogram(new TH2F(Form("hit_tot_thr2_scin_mtx_pos_%d", mtxPos),
                                             Form("Hit TOT for times at THR2 of SiPM signals on matrix position %d", mtxPos),
                                             maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 200, 0.0, 400000.0));
    getStatistics().getHisto2D(Form("hit_tot_thr2_scin_mtx_pos_%d", mtxPos))->GetXaxis()->SetTitle("Scintillator ID");
    getStatistics().getHisto2D(Form("hit_tot_thr2_scin_mtx_pos_%d", mtxPos))->GetYaxis()->SetTitle("Time over Threshold [ps]");

    getStatistics().createHistogram(new TH2F(Form("hit_tot_sum_scin_mtx_pos_%d", mtxPos),
                                             Form("Hit TOT for times at both thresholds of SiPM signals on matrix position %d", mtxPos),
                                             maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5, 200, 0.0, 400000.0));
    getStatistics().getHisto2D(Form("hit_tot_sum_scin_mtx_pos_%d", mtxPos))->GetXaxis()->SetTitle("Scintillator ID");
    getStatistics().getHisto2D(Form("hit_tot_sum_scin_mtx_pos_%d", mtxPos))->GetYaxis()->SetTitle("Time over Threshold [ps]");
  }

  // Unused sigals stats
  getStatistics().createHistogram(
      new TH1F("remain_signals_scin", "Number of Unused Signals in Scintillator", maxScinID - minScinID + 1, minScinID - 0.5, maxScinID + 0.5));
  getStatistics().getHisto1D("remain_signals_scin")->GetXaxis()->SetTitle("Scintillator ID");
  getStatistics().getHisto1D("remain_signals_scin")->GetYaxis()->SetTitle("Number of Unused Signals in Scintillator");

  getStatistics().createHistogram(
      new TH1F("remain_signals_tdiff", "Time Diff of an unused signal and the consecutive one", 200, fABTimeDiff, 5.0 * fABTimeDiff));
  getStatistics().getHisto1D("remain_signals_tdiff")->GetXaxis()->SetTitle("Time difference [ps]");
  getStatistics().getHisto1D("remain_signals_tdiff")->GetYaxis()->SetTitle("Number of Signals");
}
