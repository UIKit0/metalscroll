/*
*	Copyright 2009 Griffin Software
*
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*
*		http://www.apache.org/licenses/LICENSE-2.0
*
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License. 
*/

#include "MetalScrollPCH.h"
#include "Intervals.h"

void Intervals::Add(int start, int end)
{
	assert(start < end);

	ItemListIt startPos = std::lower_bound(m_intervals.begin(), m_intervals.end(), start);
	ItemListIt endPos = std::upper_bound(startPos, m_intervals.end(), end);

	// Determine if the start and end positions are on even (left) or odd (right) interval limits.
	int startType = (startPos - m_intervals.begin()) % 2;
	int endType = (endPos - m_intervals.begin()) % 2;

	int insVals[2];

	// If "start" is inside an interval, use the minimum of that interval
	// as the left of the inserted interval. Otherwise use "start".
	startPos -= startType;
	insVals[0] = (startType == 1) ? *startPos : start;

	// If "end" is inside an interval, use the maximum of that interval
	// as the right of the inserted interval. Otherwise use "end".
	insVals[1] = (endType == 1) ? *endPos : end;
	endPos += endType;

	// erase() will invalidate the iterators, so keep the index of the insert position.
	int startIdx = startPos - m_intervals.begin();
	// Delete all the intervals overlapped by the new interval.
	m_intervals.erase(startPos, endPos);
	// Finally insert the new interval.
	m_intervals.insert(m_intervals.begin() + startIdx, insVals, insVals + 2);
}

void Intervals::BeginScan()
{
	m_scanIt = m_intervals.begin();
	m_scanVal = 0;
}

void Intervals::NextValue()
{
	++m_scanVal;
}

bool Intervals::IsCurrentValueInside()
{
	if(m_scanIt == m_intervals.end())
		return false;

	// Are we inside the next interval yet?
	if(m_scanVal < *m_scanIt)
		return false;

	// Did we pass its left end?
	if(m_scanVal <= *(m_scanIt + 1))
		return true;

	// Go to the next interval.
	m_scanIt += 2;
	if(m_scanIt != m_intervals.end())
		return (m_scanVal >= *m_scanIt);
	else
		return false;
}
