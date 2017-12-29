/**
 *  @copyright Copyright 2017 The J-PET Framework Authors. All rights reserved.
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
 *  @file EventCategorizerImaging.h
 */

#ifndef EVENTCATEGORIZERIMAGING_H
#define EVENTCATEGORIZERIMAGING_H
#include <vector>
#include <JPetHit/JPetHit.h>
#include <JPetUserTask/JPetUserTask.h>
#include <JPetStatistics/JPetStatistics.h>

class EventCategorizerImaging
{
public:

	static int Imaging( std::vector<JPetHit> Hits, double MinAnnihilationTOT, double MaxAnnihilationTOT, JPetStatistics& Stats, bool SaveControlHistos );
};


#endif