/**
 *  @copyright Copyright 2020 The J-PET Framework Authors. All rights reserved.
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may find a copy of the License in the LICENCE file.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  @file HitFinderTools.cpp
 */

using namespace std;

#include "HitFinderTools.h"
#include <TMath.h>
#include <cmath>
#include <map>
#include <vector>

/**
 * Helper method for sotring signals in vector
 */
void HitFinderTools::sortByTime(vector<JPetMatrixSignal>& sigVec)
{
  sort(sigVec.begin(), sigVec.end(), [](const JPetMatrixSignal& sig1, const JPetMatrixSignal& sig2) { return sig1.getTime() < sig2.getTime(); });
}

/**
 * Method distributing Signals according to Scintillator they belong to
 */
map<int, vector<JPetMatrixSignal>> HitFinderTools::getSignalsByScin(const JPetTimeWindow* timeWindow)
{
  map<int, vector<JPetMatrixSignal>> signalScinMap;
  if (!timeWindow)
  {
    WARNING("Pointer of Time Window object is not set, returning empty map");
    return signalScinMap;
  }
  const unsigned int nSignals = timeWindow->getNumberOfEvents();
  for (unsigned int i = 0; i < nSignals; i++)
  {
    auto mtxSig = dynamic_cast<const JPetMatrixSignal&>(timeWindow->operator[](i));
    int scinID = mtxSig.getPM().getScin().getID();
    auto search = signalScinMap.find(scinID);
    if (search == signalScinMap.end())
    {
      vector<JPetMatrixSignal> tmp;
      tmp.push_back(mtxSig);
      signalScinMap.insert(pair<int, vector<JPetMatrixSignal>>(scinID, tmp));
    }
    else
    {
      search->second.push_back(mtxSig);
    }
  }
  return signalScinMap;
}

/**
 * Loop over all Scins invoking matching procedure
 */
vector<JPetHit> HitFinderTools::matchAllSignals(map<int, vector<JPetMatrixSignal>>& allSignals, double timeDiffAB,
                                                boost::property_tree::ptree& calibTree, JPetStatistics& stats, bool saveHistos)
{
  vector<JPetHit> allHits;
  for (auto& scinSigals : allSignals)
  {
    // Match signals for scintillators
    auto scinHits = matchSignals(scinSigals.second, timeDiffAB, calibTree, stats, saveHistos);
    allHits.insert(allHits.end(), scinHits.begin(), scinHits.end());
  }
  return allHits;
}

/**
 * Method matching signals on the same Scintillator
 */
vector<JPetHit> HitFinderTools::matchSignals(vector<JPetMatrixSignal>& scinSigals, double timeDiffAB, boost::property_tree::ptree& calibTree,
                                             JPetStatistics& stats, bool saveHistos)
{
  vector<JPetHit> scinHits;
  vector<JPetMatrixSignal> remainSignals;
  sortByTime(scinSigals);

  while (scinSigals.size() > 0)
  {
    auto mtxSig = scinSigals.at(0);
    if (scinSigals.size() == 1)
    {
      remainSignals.push_back(mtxSig);
      break;
    }

    for (unsigned int j = 1; j < scinSigals.size(); j++)
    {
      if (scinSigals.at(j).getTime() - mtxSig.getTime() < timeDiffAB)
      {
        if (mtxSig.getPM().getSide() != scinSigals.at(j).getPM().getSide())
        {

          auto hit = createHit(mtxSig, scinSigals.at(j), calibTree);

          scinHits.push_back(hit);
          scinSigals.erase(scinSigals.begin() + j);
          scinSigals.erase(scinSigals.begin() + 0);
          break;
        }
        else
        {
          if (j == scinSigals.size() - 1)
          {
            remainSignals.push_back(mtxSig);
            scinSigals.erase(scinSigals.begin() + 0);
            break;
          }
          else
          {
            continue;
          }
        }
      }
      else
      {
        if (saveHistos && mtxSig.getPM().getSide() != scinSigals.at(j).getPM().getSide())
        {
          stats.getHisto1D("remain_signals_tdiff")->Fill(scinSigals.at(j).getTime() - mtxSig.getTime());
        }
        remainSignals.push_back(mtxSig);
        scinSigals.erase(scinSigals.begin() + 0);
        break;
      }
    }
  }
  if (remainSignals.size() > 0 && saveHistos)
  {
    stats.getHisto1D("remain_signals_scin")->Fill((double)(remainSignals.at(0).getPM().getScin().getID()), remainSignals.size());
  }
  return scinHits;
}

/**
 * Method for Hit creation - setting all fields that make sense here
 */
JPetHit HitFinderTools::createHit(const JPetMatrixSignal& signal1, const JPetMatrixSignal& signal2, boost::property_tree::ptree& calibTree)
{
  int scinID = signal1.getPM().getScin().getID();

  JPetMatrixSignal signalA;
  JPetMatrixSignal signalB;

  if (signal1.getPM().getSide() == JPetPM::SideA)
  {
    signalA = signal1;
    signalB = signal2;
  }
  else
  {
    signalA = signal2;
    signalB = signal1;
  }

  // Getting constants for this scintillator
  int velCounter = 0;
  double avVelocity = 0.0;
  for (int thr = 1; thr <= 2; ++thr)
  {
    for (int mtx = 1; mtx <= 4; mtx++)
    {
      auto param = Form("%s.%d.%s%d%s%d.%s", "scin", scinID, "hit_tdiff_thr", thr, "_scin_mtx_pos_", mtx, "eff_velocity");
      double vel = calibTree.get(param, -999.9);
      if (vel != -999.9)
      {
        avVelocity += vel;
        velCounter++;
      }
    }
  }
  if (velCounter > 0)
  {
    avVelocity = avVelocity / ((double)velCounter);
  }

  double tofCorrection = calibTree.get("scin." + to_string(scinID) + ".tof_correction", 0.0);
  double totNormA = calibTree.get("scin." + to_string(scinID) + ".tot_scaling_factor_a", 1.0);
  double totNormB = calibTree.get("scin." + to_string(scinID) + ".tot_scaling_factor_b", 0.0);

  JPetHit hit;
  hit.setSignals(signalA, signalB);
  hit.setTime(((signalA.getTime() + signalB.getTime()) / 2.0) - tofCorrection);
  hit.setQualityOfTime(-1.0);
  hit.setTimeDiff(signalB.getTime() - signalA.getTime());
  hit.setQualityOfTimeDiff(-1.0);
  hit.setPosX(signalA.getPM().getScin().getCenterX());
  hit.setPosY(signalA.getPM().getScin().getCenterY());
  hit.setScin(signalA.getPM().getScin());

  if (avVelocity != 0.0)
  {
    hit.setPosZ(avVelocity * hit.getTimeDiff() / 2.0);
  }
  else
  {
    hit.setPosZ(-999.9);
  }

  // TOT of a signal is a sum of over all threshold in a signal
  auto tot = signalA.getTOT() + signalB.getTOT();
  hit.setEnergy(tot);

  // As a TOT of a hit we put avarege of all TOT of SiPM signals constructing this hit
  // that is sum of TOT divided by multiplicity. TOT value is normalized using
  // equalization constants from calibration file. We save TOT it temporaily as energy
  // As an quality of energy we put TOT value before normalization
  // auto multi = signalA.getRawSignals().size() + signalB.getRawSignals().size();
  // auto avToT = tot / ((double)multi);
  // auto normToT = avToT * totNormA + totNormB;
  // hit.setEnergy(normToT);
  // hit.setQualityOfEnergy(avToT);

  return hit;
}

/**
 * Method for a dummy Hit creation, setting only some necessary fields.
 */
JPetHit HitFinderTools::createDummyHit(const JPetMatrixSignal& signal)
{
  JPetHit hit;
  JPetMatrixSignal dummy;
  hit.setSignalA(dummy);
  hit.setSignalB(signal);
  hit.setTime(signal.getTime());
  hit.setQualityOfTime(-1.0);
  hit.setTimeDiff(0.0);
  hit.setQualityOfTimeDiff(-1.0);
  hit.setEnergy(signal.getTOT());
  hit.setQualityOfEnergy(-1.0);
  hit.setPosX(signal.getPM().getScin().getCenterX());
  hit.setPosY(signal.getPM().getScin().getCenterY());
  hit.setPosZ(0.0);
  hit.setScin(signal.getPM().getScin());
  return hit;
}

/**
 * Calculation of the total TOT of the hit - Time over Threshold:
 * the sum of the TOTs on both thresholds and on the both sides (A,B)
 */
double HitFinderTools::calculateTOT(JPetHit& hit)
{
  double tot = 0.0;

  auto rawSignalsA = dynamic_cast<const JPetMatrixSignal&>(hit.getSignalA()).getRawSignals();
  auto rawSignalsB = dynamic_cast<const JPetMatrixSignal&>(hit.getSignalB()).getRawSignals();

  for (auto rawSig : rawSignalsA)
  {
    auto sigALead = rawSig.second.getPoints(JPetSigCh::Leading, JPetRawSignal::ByThrNum);
    auto sigATrail = rawSig.second.getPoints(JPetSigCh::Trailing, JPetRawSignal::ByThrNum);
    if (sigALead.size() > 0 && sigATrail.size() > 0)
    {
      for (unsigned i = 0; i < sigALead.size() && i < sigATrail.size(); i++)
      {
        tot += (sigATrail.at(i).getTime() - sigALead.at(i).getTime());
      }
    }
  }

  for (auto rawSig : rawSignalsB)
  {
    auto sigBLead = rawSig.second.getPoints(JPetSigCh::Leading, JPetRawSignal::ByThrNum);
    auto sigBTrail = rawSig.second.getPoints(JPetSigCh::Trailing, JPetRawSignal::ByThrNum);
    if (sigBLead.size() > 0 && sigBTrail.size() > 0)
    {
      for (unsigned i = 0; i < sigBLead.size() && i < sigBTrail.size(); i++)
      {
        tot += (sigBTrail.at(i).getTime() - sigBLead.at(i).getTime());
      }
    }
  }

  return tot;
}
