/***********************************************************************************************************************
*                                                                                                                      *
* libscopeprotocols                                                                                                    *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg and contributors                                                         *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

#include "../scopehal/scopehal.h"
#include "SquelchFilter.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

SquelchFilter::SquelchFilter(const string& color)
	: Filter(color, CAT_MATH)
{
	//Set up channels
	CreateInput("in");
	ClearStreams();
	AddStream(Unit(Unit::UNIT_VOLTS), "out", Stream::STREAM_TYPE_DIGITAL);

	m_thresholdname = "Threshold";
	m_parameters[m_thresholdname] = FilterParameter(FilterParameter::TYPE_FLOAT, Unit(Unit::UNIT_VOLTS));
	m_parameters[m_thresholdname].SetFloatVal(0.01);

	m_holdtimename = "Hold time";
	m_parameters[m_holdtimename] = FilterParameter(FilterParameter::TYPE_INT, Unit(Unit::UNIT_FS));
	m_parameters[m_holdtimename].SetIntVal(1e6);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Factory methods

bool SquelchFilter::ValidateChannel(size_t i, StreamDescriptor stream)
{
	if(stream.m_channel == NULL)
		return false;

	if( (i < 1) && (stream.GetType() == Stream::STREAM_TYPE_ANALOG) )
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Accessors

string SquelchFilter::GetProtocolName()
{
	return "Squelch";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual decoder logic

void SquelchFilter::Refresh()
{
	//Make sure we've got valid inputs
	if(!VerifyAllInputsOKAndUniformAnalog())
	{
		SetData(NULL, 0);
		return;
	}

	auto din = dynamic_cast<UniformAnalogWaveform*>(GetInputWaveform(0));
	din->PrepareForCpuAccess();

	size_t len = din->size();

	auto threshold = m_parameters[m_thresholdname].GetFloatVal();
	auto holdtime_fs = m_parameters[m_holdtimename].GetIntVal();
	size_t holdtime_samples = holdtime_fs / din->m_timescale;

	auto dout = SetupEmptyUniformDigitalOutputWaveform(din, 0);
	dout->Resize(len);
	dout->PrepareForCpuAccess();

	bool open = false;
	size_t topen = 0;
	for(size_t i=0; i<len; i++)
	{
		//Signal amplitude is above threshold - open squelch immediately
		//TODO: attack time?
		if(din->m_samples[i] > threshold)
		{
			open = true;
			topen = i;
		}

		//Signal amplitude below threshold - close squelch after hold time elapses
		else if(open && ( (i - topen) > holdtime_samples) )
			open = false;

		dout->m_samples[i] = open;
	}

	dout->MarkModifiedFromCpu();
}
