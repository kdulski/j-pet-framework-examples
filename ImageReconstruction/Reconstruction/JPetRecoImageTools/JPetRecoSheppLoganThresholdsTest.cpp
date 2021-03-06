#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE JPetRecoImageToolsTests
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

#include "./JPetFilterCosine.h"
#include "./JPetFilterHamming.h"
#include "./JPetFilterNone.h"
#include "./JPetFilterRamLak.h"
#include "./JPetFilterRidgelet.h"
#include "./JPetFilterSheppLogan.h"
#include "./JPetRecoImageTools.h"
#include "JPetCommonTools/JPetCommonTools.h"

const auto inFile = "unitTestData/JPetRecoImageToolsTest/sinogramBackproject.ppm";

int getMaxValue(const JPetRecoImageTools::Matrix2DProj& result)
{
  int maxValue = 0;
  for (unsigned int i = 0; i < result.size(); i++)
  {
    for (unsigned int j = 0; j < result[0].size(); j++)
    {
      if (static_cast<int>(result[i][j]) > maxValue)
        maxValue = static_cast<int>(result[i][j]);
    }
  }
  return maxValue;
}

void saveResult(const JPetRecoImageTools::Matrix2DProj& result, const std::string& outputFileName)
{
  int maxValue = getMaxValue(result);
  std::ofstream res(outputFileName);
  res << "P2" << std::endl;
  res << result[0].size() << " " << result.size() << std::endl;
  res << maxValue << std::endl;
  for (unsigned int i = 0; i < result.size(); i++)
  {
    for (unsigned int j = 0; j < result[0].size(); j++)
    {
      int resultInt = static_cast<int>(result[i][j]);
      if (resultInt < 0)
      {
        resultInt = 0;
      }
      res << resultInt << " ";
    }
    res << std::endl;
  }
  res.close();
}

JPetRecoImageTools::Matrix2DProj readFile(const std::string& inputFile)
{
  std::ifstream in(inputFile);
  BOOST_REQUIRE(in);
  std::string line;
  getline(in, line);
  unsigned int width;
  unsigned int height;
  in >> width;
  in >> height;
  int val;
  in >> val; // skip max val
  JPetRecoImageTools::Matrix2DProj sinogram(height, std::vector<double>(width));
  for (unsigned int i = 0; i < height; i++)
  {
    for (unsigned int j = 0; j < width; j++)
    {
      in >> val;
      sinogram[i][j] = val;
    }
  }
  return sinogram;
}

BOOST_AUTO_TEST_SUITE(FirstSuite)

BOOST_AUTO_TEST_CASE(backProjectSinogramSheppLogan)
{
  for (double threshold = 0.01; threshold <= 1.; threshold += 0.1)
  {
    const auto outFile = "ramlak/backprojectSinogramRamLakT" + std::to_string(threshold) + ".ppm";

    JPetRecoImageTools::Matrix2DProj sinogram = readFile(inFile);

    JPetFilterRamLak ramlakFilter(threshold);
    JPetRecoImageTools::FourierTransformFunction f = JPetRecoImageTools::doFFTW;
    JPetRecoImageTools::Matrix2DProj filteredSinogram = JPetRecoImageTools::FilterSinogram(f, ramlakFilter, sinogram);
    JPetRecoImageTools::Matrix2DProj result =
        JPetRecoImageTools::backProject(filteredSinogram, sinogram[0].size(), JPetRecoImageTools::nonRescale, 0, 255);

    saveResult(result, outFile);

    BOOST_REQUIRE(JPetCommonTools::ifFileExisting(outFile));
  }
}

BOOST_AUTO_TEST_SUITE_END()